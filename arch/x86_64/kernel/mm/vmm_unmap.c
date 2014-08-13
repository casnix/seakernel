/* Functions for unmapping addresses */
#include <sea/mm/vmm.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/process.h>
#include <sea/mm/swap.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/cpu-x86_64.h>
#include <sea/asm/system.h>
/* send the TBL shootdown to cpus if:
 *  - tasking is enabled
 *  - either:
 * 	  - this is kernel memory
 *    - OR this is shared threading memory, and there is another thread
 *  - BUT NOT if we are unmapping the pd_cur_data page
 */

int arch_mm_vm_unmap_only(addr_t virt, unsigned locked)
{
	#if CONFIG_SWAP
	if(current_task && num_swapdev && current_task->num_swapped)
		swap_in_page((task_t *)current_task, virt & PAGE_MASK);
	#endif
	addr_t vpage = (virt&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);
	if(kernel_task && (virt&PAGE_MASK) != PDIR_DATA && !locked)
		mutex_acquire(&pd_cur_data->lock);
	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	pml4_t *pml4;
	
	pml4 = (pml4_t *)((kernel_task && current_task) ? current_task->pd : kernel_dir);
	if(!pml4[vp4])
		pml4[vp4] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
	pdpt = (addr_t *)((pml4[vp4]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdpt[vpdpt])
		pdpt[vpdpt] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
	pd = (addr_t *)((pdpt[vpdpt]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pd[vdir])
		pd[vdir] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
	pt = (addr_t *)((pd[vdir]&PAGE_MASK) + PHYS_PAGE_MAP);
	
	pt[vtbl] = 0;
	asm("invlpg (%0)"::"r" (virt));
	#if CONFIG_SMP
	if(kernel_task && (virt&PAGE_MASK) != PDIR_DATA) {
		if(IS_KERN_MEM(virt))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(virt) && pd_cur_data->count > 1))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
	#endif
	if(kernel_task && (virt&PAGE_MASK) != PDIR_DATA && !locked)
		mutex_release(&pd_cur_data->lock);
	return 0;
}

int arch_mm_vm_unmap(addr_t virt, unsigned locked)
{
	/* This gives the virtual address of the table needed, and sets
	 * the correct place as zero */
	#if CONFIG_SWAP
	if(current_task && num_swapdev && current_task->num_swapped)
		swap_in_page((task_t *)current_task, virt & PAGE_MASK);
	#endif
	addr_t vpage = (virt&PAGE_MASK)/0x1000;
	unsigned vp4 = PML4_IDX(vpage);
	unsigned vpdpt = PDPT_IDX(vpage);
	unsigned vdir = PAGE_DIR_IDX(vpage);
	unsigned vtbl = PAGE_TABLE_IDX(vpage);
	if(kernel_task && (virt&PAGE_MASK) != PDIR_DATA && !locked)
		mutex_acquire(&pd_cur_data->lock);
	page_dir_t *pd;
	page_table_t *pt;
	pdpt_t *pdpt;
	pml4_t *pml4;
	
	pml4 = (pml4_t *)((kernel_task && current_task) ? current_task->pd : kernel_dir);
	if(!pml4[vp4])
		pml4[vp4] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
	pdpt = (addr_t *)((pml4[vp4]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pdpt[vpdpt])
		pdpt[vpdpt] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
	pd = (addr_t *)((pdpt[vpdpt]&PAGE_MASK) + PHYS_PAGE_MAP);
	if(!pd[vdir])
		pd[vdir] = mm_alloc_physical_page() | PAGE_PRESENT | PAGE_WRITE;
	pt = (addr_t *)((pd[vdir]&PAGE_MASK) + PHYS_PAGE_MAP);
	
	addr_t p = pt[vtbl];
	pt[vtbl] = 0;
	asm("invlpg (%0)"::"r" (virt));
	#if CONFIG_SMP
	if(kernel_task && (virt&PAGE_MASK) != PDIR_DATA) {
		if(IS_KERN_MEM(virt))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
		else if((IS_THREAD_SHARED_MEM(virt) && pd_cur_data->count > 1))
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_OTHERS, 0, LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | IPI_TLB);
	}
	#endif
	if(kernel_task && (virt&PAGE_MASK) != PDIR_DATA && !locked)
		mutex_release(&pd_cur_data->lock);
	if(p && !(p & PAGE_COW))
		mm_free_physical_page(p & PAGE_MASK);
	return 0;
}
