/* Defines functions for virtual memory management */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/interrupt.h>
volatile addr_t *kernel_dir=0;
pml4_t *kernel_dir_phys=0;
int id_tables=0;
struct pd_data *pd_cur_data = (struct pd_data *)PDIR_DATA;

/* This function will setup a paging environment with a basic page dir, 
 * enough to process the memory map passed by grub */
static void early_mm_vm_map(pml4_t *pml4, addr_t addr, addr_t map)
{
	pdpt_t *pdpt;
	page_dir_t *pd;
	page_table_t *pt;
	
	if(!pml4[PML4_IDX(addr/0x1000)])
		pml4[PML4_IDX(addr/0x1000)] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	pdpt = (addr_t *)(pml4[PML4_IDX(addr/0x1000)] & PAGE_MASK);
	if(!pdpt[PDPT_IDX(addr/0x1000)])
		pdpt[PDPT_IDX(addr/0x1000)] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	pd = (addr_t *)(pdpt[PDPT_IDX(addr/0x1000)] & PAGE_MASK);
	if(!pd[PAGE_DIR_IDX(addr/0x1000)])
		pd[PAGE_DIR_IDX(addr/0x1000)] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	/* passing map as zero allows us to map in all the tables, but leave the
	 * true mapping null. This is handy for the page stack and heap */
	pt = (addr_t *)(pd[PAGE_DIR_IDX(addr/0x1000)] & PAGE_MASK);
	pt[PAGE_TABLE_IDX(addr/0x1000)] = map;
}

static pml4_t *create_initial_directory()
{
	/* Create kernel directory */
	pml4_t *pml4 = (addr_t *)arch_mm_alloc_physical_page_zero();
	memset(pml4, 0, 0x1000);
	/* Identity map the kernel */
	pml4[0] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_USER;
	pdpt_t *pdpt = (addr_t *)(pml4[0] & PAGE_MASK);
	pdpt[0] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_USER;
	page_dir_t *pd = (addr_t *)(pdpt[0] & PAGE_MASK);
	
	addr_t address = 0;
	for(int pdi = 0; pdi < 512; pdi++)
	{
		pd[pdi] = address | PAGE_PRESENT | PAGE_USER | (1 << 7);
		address += 0x200000;
	}

	/* map in all possible physical memory, up to 512 GB. This way we can
	 * access any physical page by simple accessing virtual memory (phys + PHYS_PAGE_MAP).
	 * This should make mapping memory a LOT easier */
	pml4[PML4_IDX(PHYS_PAGE_MAP/0x1000)] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE;
	pdpt = (addr_t *)(pml4[PML4_IDX(PHYS_PAGE_MAP/0x1000)] & PAGE_MASK);

	address = 0;
	for(int i = 0; i < 512; i++)
	{
		pdpt[i] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
		pd = (addr_t *)(pdpt[i] & PAGE_MASK);
		for(int j = 0; j < 512; j++) {
			pd[j] = address | PAGE_PRESENT | PAGE_WRITE | (1 << 7);
			address += 0x200000;
		}
	}
	/* map in the signal return inject code. we need to do this, because
	 * user code may not run the the kernel area of the page directory */
	early_mm_vm_map(pml4, SIGNAL_INJECT, arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_USER);
	early_mm_vm_map(pml4, PDIR_DATA, arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE);
	
	/* CR3 requires the physical address, so we directly 
	 * set it because we have the physical address */
	printk(0, "Setting new CR3...\n");
	asm("mov %0, %%cr3"::"r"(pml4));
	return pml4;
}

void arch_mm_vm_init(addr_t id_map_to)
{
	/* Register some stuff... */
	interrupt_register_handler (14, (isr_t)&arch_mm_page_fault, 0);
 	kernel_dir = create_initial_directory();
	/* Enable paging */
	printk(0, "Paging enabled!\n");
	memcpy((void *)SIGNAL_INJECT, (void *)signal_return_injector, SIGNAL_INJECT_SIZE);
	set_ksf(KSF_PAGING);
}

/* This relocates the stack to a safe place which is copied 
 * upon clone, and creates a new directory that is...well, complete */
void arch_mm_vm_init_2()
{
	setup_kernelstack(id_tables);
	printk(0, "[mm]: cloning directory for primary cpu\n");
	primary_cpu->kd = mm_vm_clone((addr_t *)kernel_dir, 0);
	primary_cpu->kd_phys = primary_cpu->kd[PML4_IDX(PHYSICAL_PML4_INDEX/0x1000)] & PAGE_MASK;
	kernel_dir_phys = (pml4_t *)primary_cpu->kd_phys;
	kernel_dir = primary_cpu->kd;
	asm ("mov %0, %%cr3" : : "r" (kernel_dir[PML4_IDX((PHYSICAL_PML4_INDEX/0x1000))]));
}

void arch_mm_vm_switch_context(addr_t *n/*VIRTUAL ADDRESS*/)
{
	asm ("mov %0, %%cr3" : : "r" (n[PML4_IDX((PHYSICAL_PML4_INDEX/0x1000))]));
}

addr_t arch_mm_vm_get_map(addr_t v, addr_t *p, unsigned locked)
{
	addr_t vpage = (v&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);
	addr_t ret=0;
	if(kernel_task && !locked)
		mutex_acquire(&pd_cur_data->lock);
	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	pml4_t *pml4;
	
	pml4 = (pml4_t *)((kernel_task && current_task) ? current_task->pd : kernel_dir);
	if(!pml4[vp4])
		goto out;
	pdpt = (addr_t *)((pml4[vp4]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdpt[vpdpt])
		goto out;
	if(pdpt[vpdpt] & PAGE_LARGE)
	{
		ret = pdpt[vpdpt] & PAGE_MASK;
		goto out;
	}
	pd = (addr_t *)((pdpt[vpdpt]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pd[vdir])
		goto out;
	if(pd[vdir] & PAGE_LARGE)
	{
		ret = pd[vdir] & PAGE_MASK;
		goto out;
	}
	pt = (addr_t *)((pd[vdir]&PAGE_MASK) + PHYS_PAGE_MAP);
	ret = pt[vtbl] & PAGE_MASK;
	out:
	if(p)
		*p = ret;
	if(kernel_task && !locked)
		mutex_release(&pd_cur_data->lock);
	return ret;
}

void arch_mm_vm_set_attrib(addr_t v, short attr)
{
	addr_t vpage = (v&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);
	if(kernel_task)
		mutex_acquire(&pd_cur_data->lock);
	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	pml4_t *pml4;
	
	pml4 = (pml4_t *)((kernel_task && current_task) ? current_task->pd : kernel_dir);
	if(!pml4[vp4])
		pml4[vp4] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pdpt = (addr_t *)((pml4[vp4]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(pdpt[vpdpt] & PAGE_LARGE)
	{
		pdpt[vpdpt] &= PAGE_MASK;
		pdpt[vpdpt] |= (attr | PAGE_LARGE);
		goto out;
	}
	if(!pdpt[vpdpt])
		pdpt[vpdpt] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pd = (addr_t *)((pdpt[vpdpt]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(pd[vdir] & PAGE_LARGE)
	{
		pd[vdir] &= PAGE_MASK;
		pd[vdir] |= (attr | PAGE_LARGE);
		goto out;
	}
	if(!pd[vdir])
		pd[vdir] = arch_mm_alloc_physical_page_zero() | PAGE_PRESENT | PAGE_WRITE | (attr & PAGE_USER);
	pt = (addr_t *)((pd[vdir]&PAGE_MASK) + PHYS_PAGE_MAP);
	
	pt[vtbl] &= PAGE_MASK;
	pt[vtbl] |= attr;
	out:
	asm("invlpg (%0)"::"r" (v));
#if CONFIG_SMP && 0
	if(kernel_task) {
		if(IS_KERN_MEM(v))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(v) && pd_cur_data->count > 1))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
#endif
	if(kernel_task)
		mutex_release(&pd_cur_data->lock);
	return 0;
}

unsigned int arch_mm_vm_get_attrib(addr_t v, unsigned *p, unsigned locked)
{
	addr_t vpage = (v&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);
	unsigned ret=0;
	if(kernel_task && !locked)
		mutex_acquire(&pd_cur_data->lock);
	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	pml4_t *pml4;
	
	pml4 = (pml4_t *)((kernel_task && current_task) ? current_task->pd : kernel_dir);
	if(!pml4[vp4])
		goto out;
	pdpt = (addr_t *)((pml4[vp4]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdpt[vpdpt])
		goto out;
	if(pdpt[vpdpt] & PAGE_LARGE)
	{
		ret = pdpt[vpdpt] & ATTRIB_MASK;
		goto out;
	}
	pd = (addr_t *)((pdpt[vpdpt]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pd[vdir])
		goto out;
	if(pd[vdir] & PAGE_LARGE)
	{
		ret = pd[vdir] & ATTRIB_MASK;
		goto out;
	}
	pt = (addr_t *)((pd[vdir]&PAGE_MASK) + PHYS_PAGE_MAP);
	ret = pt[vtbl] & ATTRIB_MASK;
	out:
	if(p)
		*p = ret;
	if(kernel_task && !locked)
		mutex_release(&pd_cur_data->lock);
	return ret;
}
