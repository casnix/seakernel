#ifndef _SYSCALL_X86_H
#define _SYSCALL_X86_H
#define SC (int (*)(int, int, int, int, int))
#include <sea/asm/system.h>
static int dosyscall(int num, int a, int b, int c, int d, int e)
{
	int x;
	asm("int $0x80":"=a"(x):"0" (num), "b" ((int)a), "c" ((int)b), "d" ((int)c), "S" ((int)d), "D" ((int)e));
	return x;
}
#define __do_syscall_jump(ret, location, a, b, c, d, e) __asm__ __volatile__(" \
	push %1; \
	push %2; \
	push %3; \
	push %4; \
	push %5; \
	call *%6; \
	pop %%ebx; \
	pop %%ebx; \
	pop %%ebx; \
	pop %%ebx; \
	pop %%ebx; \
	" \
	: "=a" (ret) \
	: "r" (a), "r" (b), "r" (c), "r" (d), "r" (e), "r" (location))

#define SYSCALL_NUM_AND_RET regs->eax
#define _E_ regs->edi
#define _D_ regs->esi
#define _C_ regs->edx
#define _B_ regs->ecx
#define _A_ regs->ebx

#define SIGNAL_INJECT_SIZE 7
static unsigned char signal_return_injector[SIGNAL_INJECT_SIZE] = {
	0xB8,
	0x80,
	0x00,
	0x00,
	0x00,
	0xCD,
	0x80
};

#endif
