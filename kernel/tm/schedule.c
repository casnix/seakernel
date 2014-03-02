#include <sea/subsystem.h>
#define SUBSYSTEM _SUBSYSTEM_TM
#include <sea/tm/_tm.h>
#include <sea/tm/process.h>
/* Functions for scheduling tasks */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <cpu.h>
#include <context.h>

static __attribute__((always_inline)) inline void update_task(task_t *t)
{
	/* task's tm_delay ran out */
	if((t->state == TASK_USLEEP || t->state == TASK_ISLEEP) && t->tick <= ticks && t->tick)
		t->state = TASK_RUNNING;
}
/* This here is the basic scheduler - It does nothing 
 * except find the next runable task */
static __attribute__((always_inline)) inline task_t *get_next_task(task_t *prev, cpu_t *cpu)
{
	assert(prev && kernel_task);
	assert(prev->cpu == cpu);
	assert(cpu);
	cpu = prev->cpu;
	task_t *t = tqueue_next(cpu->active_queue);
	while(t)
	{
		assert(t);
		if(unlikely(t->magic != TASK_MAGIC))
			panic(0, "Invalid task (%d:%d): %x", t->pid, t->state, t->magic);
		/* this handles everything in the "active queue". This includes
		 * running tasks, tasks that have timed blocks... */
		update_task(t);
		if(__tm_process_is_runable(t) && !(t->flags & TF_MOVECPU))
			return t;
		t = tqueue_next(cpu->active_queue);
		/* This way the kernel can sleep without being in danger of 
		 * causing a lockup. Basically, if the kernel is the only
		 * runnable task, it gets forced to run */
		if(t && t == prev && !__tm_process_is_runable(t)) {
			/* make sure to update the state in case it slept */
			assert(cpu->ktask);
			cpu->ktask->state = TASK_RUNNING;
			return (task_t *)cpu->ktask;
		}
	}
	panic(PANIC_NOSYNC, "get_next_task(): Task became null pointer!", t);
	return (task_t *)0;
}

__attribute__((always_inline)) static inline void post_context_switch()
{
	if(unlikely(current_task->state == TASK_SUICIDAL) && !(current_task->flags & TF_EXITING))
		tm_process_suicide();
	/* We only process signals if we aren't in a system call.
	 * this is because if a task is suddenly interrupted inside an
	 * important syscall while doing something important the results
	 * could be very bad. Any syscall that waits will need to include
	 * a method of detecting signals and returning safely. */
	if(current_task->sigd 
		&& (!(current_task->flags & TF_INSIG) 
		   || tm_signal_will_be_fatal(current_task, current_task->sigd))
		&& !(current_task->flags & TF_KTASK) && current_task->pid
		&& !(current_task->flags & TF_EXITING) && !(current_task->system == SYS_FORK))
	{
		tm_raise_flag(TF_INSIG);
		/* Jump to the signal handler */
		__tm_handle_signal((task_t *)current_task);
		/* if we've gotten here, then we are interruptible or running.
		 * set the state to running since interruptible tasks fully
		 * wake up when signaled */
		current_task->state = TASK_RUNNING;
	}
	assert(!(kernel_state_flags & KSF_SHUTDOWN) || current_task->flags & TF_SHUTDOWN);
	assert(!get_cpu_interrupt_flag());
	if(current_task->flags & TF_SETINT) {
		/* should never enable interrupts inside an interrupt, except for
		 * syscalls */
		assert(!(current_task->flags & TF_IN_INT) || current_task->sysregs);
		tm_lower_flag(TF_SETINT);
		assert(!interrupt_set(1));
	}
}

int tm_schedule()
{
	if(unlikely(!current_task || !kernel_task))
		return 0;
	if((((cpu_t *)current_task->cpu)->flags & CPU_LOCK))
		return 0;
	if(!(((cpu_t *)current_task->cpu)->flags & CPU_TASK))
		return 0;
	assert(!(kernel_state_flags & KSF_SHUTDOWN) || current_task->flags & TF_SHUTDOWN);
	if(kernel_state_flags & KSF_SHUTDOWN) return 1;
	assert(current_task->magic == TASK_MAGIC);
	if(current_task->thread) assert(current_task->thread->magic == THREAD_MAGIC);
	/* make sure to re-enable interrupts when we come back to this
	 * task if we entered schedule with them enabled */
	if(interrupt_set(0)) {
		assert(!(current_task->flags & TF_SETINT));
		tm_raise_flag(TF_SETINT);
	} else
		assert(!(current_task->flags & TF_SETINT));
	task_t *old = current_task;
	cpu_t *cpu = (cpu_t *)old->cpu;
	assert(cpu && cpu->cur == old);
	
	mutex_acquire(&cpu->lock);
	store_context();
	/* the tm_exiting task has fully 'exited' and has now scheduled out of
	 * itself. It will never be scheduled again, and the page directory
	 * will never be accessed again */
	if(old->flags & TF_DYING) {
		assert(old->state == TASK_DEAD);
		tm_raise_flag(TF_BURIED);
	}
	old->syscall_count = 0;
	task_t *next_task = (task_t *)get_next_task(old, cpu);
	assert(next_task);
	assert(cpu == next_task->cpu);
	restore_context(next_task);
	next_task->slice = ticks;
	((cpu_t *)next_task->cpu)->cur = next_task;
	
	/* we need to call this after restore_context because in restore_context
	 * we access new->cpu */
	mutex_release(&cpu->lock);
	/* after calling context switch, we may NOT use any variables that
	 * were used above it, since they are not necessarily valid. */
	context_switch(next_task);
	assert(current_task->magic == TASK_MAGIC);
	if(current_task->thread) assert(current_task->thread->magic == THREAD_MAGIC);
	//reset_timer_state(); /* TODO: This may be needed... */
	/* tasks that have come from fork() (aka, new tasks) have this
	 * flag set, such that here we just jump to their entry point in fork() */
	if(likely(!(current_task->flags & TF_FORK)))
	{
		post_context_switch();
		return 1;
	}
	tm_process_lower_flag(current_task, TF_FORK);
	interrupt_set(1);
	asm("jmp *%0"::"r"(current_task->eip));
	/* we never get here, but lets keep gcc happy */
	return 1;
}

void __tm_check_alarms()
{
	if(!alarm_list_start) return;
	/* interrupts will be disabled here. Thus, we can aquire 
	 * a mutex safely */
	mutex_acquire(alarm_mutex);
	if((unsigned)ticks > alarm_list_start->alarm_end)
	{
		tm_process_lower_flag(alarm_list_start, TF_ALARM);
		alarm_list_start->sigd = SIGALRM;
		alarm_list_start = alarm_list_start->alarm_next;
		alarm_list_start->alarm_prev = 0;
	}
	mutex_release(alarm_mutex);
}
