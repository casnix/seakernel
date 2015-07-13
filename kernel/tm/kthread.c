#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/tm/kthread.h>
#include <sea/mm/vmm.h>
#include <sea/mm/kmalloc.h>
#include <sea/cpu/atomic.h>
#include <sea/string.h>
#include <sea/boot/init.h>

struct kthread *kthread_create(struct kthread *kt, const char *name, int flags,
		int (*entry)(struct kthread *, void *), void *arg)
{
	if(!kt) {
		kt = kmalloc(sizeof(struct kthread));
		kt->flags = KT_ALLOC;
	} else {
		kt->flags = 0;
	}

	kt->flags |= flags;
	kt->entry = entry;
	kt->arg = arg;
	int tid = sys_clone(CLONE_SHARE_PROCESS);
	if(!tid) {
		/* instead of being part of whichever process created us, we need
		 * to be part of the kernel process */
		struct process *oldproc = current_process;
		if(oldproc != kernel_process) {
			ll_do_remove(&oldproc->threadlist, &current_thread->pnode, 0);
			tm_process_put(oldproc);
			sub_atomic(&oldproc->thread_count, 1);
			current_thread->process = kernel_process;
		}

		current_thread->flags |= THREAD_KERNEL;
		current_thread->system = -1;
		current_thread->regs = 0;
		strncpy((char *)current_process->command, name, 128);
		
		/* okay, call the thread entry function */
		kt->code = entry(kt, arg);
		/* get the code FIRST, since once we set KT_EXITED, we can't rely on kt
		 * being a valid pointer */
		int code = kt->code;
		or_atomic(&kt->flags, KT_EXITED);
		tm_thread_exit(0);
		panic(0, "kthread lived past exit");
	}
	kt->thread = tm_thread_get(tid);
	return kt;
}

void kthread_destroy(struct kthread *kt)
{
	if(!(kt->flags & KT_EXITED))
		panic(0, "tried to destroy a running kernel thread");
	if(kt->flags & KT_ALLOC)
		kfree(kt);
}

int kthread_wait(struct kthread *kt, int flags)
{
	/* eh, the thread may wish to know this */
	or_atomic(&kt->flags, KT_WAITING);
	while(!(kt->flags & KT_EXITED)) {
		/* if we don't want to block, and the thread hasn't exited,
		 * then return -1 */
		if(flags & KT_WAIT_NONBLOCK) {
			and_atomic(&kt->flags, ~KT_WAITING);
			return -1;
		}
		tm_schedule();
	}
	/* the thread has exited. return the exit code */
	and_atomic(&kt->flags, ~KT_WAITING);
	return kt->code;
}

int kthread_join(struct kthread *kt, int flags)
{
	or_atomic(&kt->flags, KT_JOIN);
	tm_thread_resume(kt->thread); /* in case it's sleeping */
	if(!(flags & KT_JOIN_NONBLOCK))
		kthread_wait(kt, 0);
	if(kt->flags & KT_EXITED)
		return kt->code;
	return -1;
}

void kthread_kill(struct kthread *kt, int flags)
{
	if(kt->thread == current_thread)
		panic(0, "kthread tried to commit suicide");
	if(!kt->thread)
		panic(0, "cannot kill unknown kthread");
	tm_signal_send_thread(kt->thread, SIGKILL);
}

