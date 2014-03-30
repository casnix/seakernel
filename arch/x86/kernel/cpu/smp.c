#include <sea/config.h>
#if CONFIG_SMP
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/mutex.h>
#include <sea/cpu/processor.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/atomic.h>
#include <imps-x86.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/cmos-x86_common.h>
#include <cpu-x86.h>
unsigned num_cpus=0, num_booted_cpus=0, num_failed_cpus=0;
int imps_scan_mptables(unsigned addr, unsigned len);
volatile unsigned num_halted_cpus=0;
int probe_smp_mptables()
{
	unsigned long long lapic_msr = read_msr(0x1b);
	write_msr(0x1b, (lapic_msr&0xFFFFF000) | 0x800, 0); //set global enable bit for lapic
	unsigned mem_lower = ((CMOS_READ_BYTE(CMOS_BASE_MEMORY+1) << 8) | CMOS_READ_BYTE(CMOS_BASE_MEMORY)) << 10;
	int res=0;
	if(mem_lower < 512*1024 || mem_lower > 640*1024)
		return 0;
	if((unsigned)EBDA_SEG_ADDR > mem_lower - 1024 || (unsigned)EBDA_SEG_ADDR + *((unsigned char *)EBDA_SEG_ADDR) * 1024 > mem_lower)
		res=imps_scan_mptables(mem_lower - 1024, 1024);
	else
		res=imps_scan_mptables(EBDA_SEG_ADDR, 1024);
	if(!res)
		res=imps_scan_mptables(0xF0000, 0x10000);
	if(!res)
		return 0;
	return 1;
}

int probe_smp()
{
	if(!probe_smp_mptables()) {
		return 0;
		//if(!parse_acpi_madt()) return 0;
	}
	set_ksf(KSF_CPUS_RUNNING);
	printk(5, "[cpu]: CPU%s initialized (boot=%d, #APs=%d: ok)                    \n", num_cpus > 1 ? "s" : "", primary_cpu->apicid, num_booted_cpus);
	return num_booted_cpus > 0;
}

#endif
