#include <sea/asm/system.h>
#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>


static struct thread *get_next_thread (void)
{
	struct thread *n = 0;
	while(1) {
		n = tqueue_next(current_thread->cpu->active_queue);
		assert(n->cpu == current_thread->cpu);
		if(n && tm_thread_runnable(n))
			break;
		if(!n || n == current_thread) {
			n = current_thread->cpu->idle_thread;
			break;
		}
	}
	assert(n && tm_thread_runnable(n));
	return n;
}

static void prepare_schedule(void)
{
	/* store arch-indep context */
	tm_thread_lower_flag(current_thread, THREAD_SCHEDULE);
}

static void finish_schedule(void)
{
	/* restore arch-indep context */
	/* check signals */
	if(current_thread->signal && !current_thread->system)
		tm_thread_handle_signal(current_thread->signal);
}

void tm_schedule(void)
{
	int old = cpu_interrupt_set(0);
	if(__current_cpu->preempt_disable > 0 || !(__current_cpu->flags & CPU_RUNNING)) {
		cpu_interrupt_set(old);
		return;
	}
	cpu_disable_preemption();
	prepare_schedule();
	struct thread *next = get_next_thread();

	if(current_thread != next) {
		arch_tm_thread_switch(current_thread, next);
	}

	struct cpu *thiscpu = current_thread->cpu;
	finish_schedule();
	cpu_enable_preemption();
	cpu_interrupt_set(old);
}

