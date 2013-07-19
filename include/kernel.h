/* every file should include this. This defines the basic things needed for
 * kernel operation. asm is redefined here, which is vital. */

#ifndef KERNEL_H
#define KERNEL_H
#include <config.h>
#include <cpu-io.h>
#include <string.h>
#include <vsprintf.h>
#include <console.h>
#include <memory.h>
#include <syscall.h>
#include <time.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <mutex.h>

#define KSF_MMU            0x1
#define KSF_SHUTDOWN       0x2
#define KSF_PANICING       0x4
#if CONFIG_SMP
  #define KSF_CPUS_RUNNING 0x8
  #define KSF_SMP_ENABLE   0x10
#endif
#define KSF_PAGING         0x20
extern volatile unsigned kernel_state_flags;
#define set_ksf(flag) {or_atomic(&kernel_state_flags, flag);}

extern volatile unsigned int __allow_idle;

#define PANIC_NOSYNC 1
#define PANIC_MEM    2
#define __UTSNAMELEN 65

struct utsname {
    char sysname[__UTSNAMELEN];
    char nodename[__UTSNAMELEN];
    char release[__UTSNAMELEN];
    char version[__UTSNAMELEN];
    char machine[__UTSNAMELEN];
    char domainname[__UTSNAMELEN];
};

#define assert(c) if(__builtin_expect((!(c)),0)) panic_assert(__FILE__, __LINE__, #c)

static inline void get_kernel_version(char *b)
{
	char t = 'a';
	if(PRE_VER >= 4 && PRE_VER < 8)
		t = 'b';
	if(PRE_VER >= 8 && PRE_VER < 10)
		t = 'c';
	if(PRE_VER == 10)
		t=0;
	int p=0;
	if(PRE_VER < 8)
		p = (PRE_VER % 4)+1;
	else
		p = (PRE_VER - 7);
	sprintf(b, "%d.%d%c%c%d", MAJ_VER, MIN_VER, t ? '-' : 0, t, p);
}

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

void print_trace(unsigned int MaxFrames);
void panic(int flags, char *fmt, ...);
void serial_puts(int, char *);
void kernel_reset();
void panic_assert(const char *file, u32int line, const char *desc);
void kernel_poweroff();
int sys_uname(struct utsname *name);
int get_timer_th(int *t);
extern int april_fools;
int sys_isstate(int pid, int state);
void do_reset();
int sys_gethostname(char *buf, size_t len);
void restart_int();
int proc_append_buffer(char *buffer, char *data, int off, int len, 
	int req_off, int req_len);
#endif
