/* init/main.c: Copyright (c) 2010 Daniel Bittman
 * Provides initialization functions for the kernel */
#include <sea/kernel.h>
#include <sea/boot/multiboot.h>
#include <sea/tty/terminal.h>
#include <sea/mm/vmm.h>
#include <sea/asm/system.h>
#include <sea/tm/process.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/boot/init.h>
#include <sea/lib/cache.h>
#include <sea/loader/symbol.h>
#include <sea/loader/elf.h>
#include <sea/cpu/processor.h>
#include <sea/mm/init.h>
#include <sea/dm/dev.h>
#include <sea/fs/initrd.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/atomic.h>

struct multiboot *mtboot;
addr_t i_stack=0;
char *stuff_to_pass[128];
int argc_STP=3;
unsigned init_pid=0;
elf32_t kernel_elf;
int april_fools=0;

static char init_path[128] = "*";
static char root_device[64] = "/";
static int count_ie=0;
static char *init_env[12];
static char cleared_args=0;
static char kernel_name[128];
static struct tm kernel_start_time;

void parse_kernel_cmd(char *buf)
{
	char *current = buf;
	char *tmp;
	unsigned argc=0;
	char a[128];
	int type=0;
	int init_mods=0;
	memset(stuff_to_pass, 0, 128 * sizeof(char *));
	while(current && *current)
	{
		tmp = strchr(current, ' ');
		memset(a, 0, 128);
		addr_t len = (addr_t)tmp ? (addr_t)(tmp-current) 
			: (addr_t)strlen(current);
		strncpy(a, current, len >= 128 ? 127 : len);
		if(!argc)
		{
			memset(kernel_name, 0, 128);
			strncpy(kernel_name, a, 128);
		} else if(!type) {
			if(!strncmp("init=\"", a, 6))
			{
				strncpy(init_path, a+6, 128);
				init_path[strlen(init_path)-1]=0;
				printk(KERN_INFO, "[kernel]: init=%s\n", init_path);
			}
			else if(!strncmp("root=\"", a, 6))
			{
				memset(root_device, 0, 64);
				strncpy(root_device, a+6, 64);
				root_device[strlen(root_device)-1]=0;
				printk(KERN_INFO, "[kernel]: root=%s\n", root_device);
			}
			else if(!strcmp("aprilfools", a))
				april_fools = !april_fools;
			else if(!strncmp("loglevel=", a, 9))
			{
				char *lev = ((char *)a) + 9;
				int logl = strtoint(lev);
				printk(1, "[kernel]: Setting loglevel to %d\n", logl);
				PRINT_LEVEL = logl;
			} else {
				stuff_to_pass[argc_STP] = (char *)kmalloc(strlen(a)+1);
				_strcpy(stuff_to_pass[argc_STP++], a);
			}
		} else
		{
			/* switch type */
			type=0;
		}
		if(!tmp)
			break;
		argc++;
		current = tmp+1;
	}
	stuff_to_pass[0] = (char *)kmalloc(9);
	_strcpy(stuff_to_pass[0], "ird-sh");
	stuff_to_pass[1] = (char *)kmalloc(9);
	_strcpy(stuff_to_pass[1], "-c");
	stuff_to_pass[2] = (char *)kmalloc(90);
	snprintf(stuff_to_pass[2], 90, "/preinit.sh %s", root_device);
	
}
/* This is the C kernel entry point */
void kmain(struct multiboot *mboot_header, addr_t initial_stack)
{
	/* Store passed values, and initiate some early things
	 * We want serial log output as early as possible */
	kernel_state_flags=0;
	mtboot = mboot_header;
	i_stack = initial_stack;
	loader_parse_kernel_elf(mboot_header, &kernel_elf);
#if CONFIG_MODULES
	loader_init_kernel_symbols();
#endif
	serial_init();
	console_init_stage1();
	cpu_early_init();
	console_kernel_puts("~ SeaOS Version ");	
	char ver[32];
	get_kernel_version(ver);
	console_kernel_puts(ver);
	console_kernel_puts(" Booting Up ~\n\r");
#if CONFIG_MODULES
	loader_init_modules();
#endif
	init_syscalls();
	fs_initrd_load(mtboot);
	cpu_timer_install(1000);
	mm_pm_init(placement, mtboot);
	cpu_processor_init_1();

	/* Now get the management stuff going */
	printk(1, "[kernel]: Starting system management\n");
	mm_init(mtboot);
	cpu_processor_init_2();
	console_init_stage2();
	parse_kernel_cmd((char *)(addr_t)mtboot->cmdline);
	tm_init_multitasking();
	init_cache();
	dm_init();
	fs_init();
	net_init();
	/* Load the rest... */
	fs_initrd_parse();
	kt_init_kernel_tasking();
	arch_time_get(&kernel_start_time);
	printk(KERN_MILE, "[kernel]: Kernel is setup (%2.2d:%2.2d:%2.2d, kv=%d, ts=%d bytes, bits=%d: ok)\n", 
	       kernel_start_time.tm_hour, kernel_start_time.tm_min, 
	       kernel_start_time.tm_sec, KVERSION, sizeof(task_t), BITS_PER_LONG);
	assert(!cpu_interrupt_set(1));
	if(!tm_fork())
		init();
	sys_setsid();
	tm_process_enter_system(255);
	kt_kernel_idle_task();
}

/* User-mode printf function */
void printf(const char *fmt, ...)
{
	char printbuf[1024];
	memset(printbuf, 0, 1024);
	va_list args;
	va_start(args, fmt);
	vsnprintf(1024, printbuf, fmt, args);
	u_write(1, printbuf);
	va_end(args);
}

void init()
{
	/* Call sys_setup. This sets up the root nodes, and filedesc's 0, 1 and 2. */
	sys_setup();
	kprintf("Something stirs and something tries, and starts to climb towards the light.\n");
	/* Set some basic environment variables. These allow simple root execution, 
	 * basic terminal access, and a shell to run from */
	add_init_env("PATH=/bin/:/usr/bin/:/usr/sbin:");
	add_init_env("TERM=seaos");
	add_init_env("HOME=/");
	add_init_env("SHELL=/bin/sh");
	int ret=0;
	int pid;
	init_pid = current_task->pid+1;
	cpu_interrupt_set_flag(1);
	tm_switch_to_user_mode();
	/* We have to be careful now. If we try to call any kernel functions
	 * without doing a system call, the processor will generate a GPF (or 
	 * a page fault) because you can't execute kernel code in ring 3!
	 * So we write simple wrapper functions for common functions that 
	 * we will need */
	ret = u_execve("/sh", (char **)stuff_to_pass, (char **)init_env);
	unset_ksf(KSF_HAVEEXECED);
	ret = u_execve("/bin/sh", (char **)stuff_to_pass, (char **)init_env);
	unset_ksf(KSF_HAVEEXECED);
	ret = u_execve("/usr/bin/sh", (char **)stuff_to_pass, (char **)init_env);
	unset_ksf(KSF_HAVEEXECED);
	printf("Failed to start the init process. Halting.\n");
	u_exit(0);
}

