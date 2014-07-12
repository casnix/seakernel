#include <sea/kernel.h>
#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/atomic.h>
/* Each page of the inode has a count associated with it. When the count reaches
 * zero, the page is written to disk, and the page is freed. When region is mapped,
 * the pages aren't necessarily allocated right away. A call to map_region
 * is made, each page in the region has its count increased. The page is only allocated
 * after a pagefault. A call to unmap_region decreases the counts of all pages in the
 * region.
 */

struct physical_page {
	addr_t page;
	int count;
	mutex_t lock;
};

/* if physicals hasn't been initialized, initialize it. */
static void __init_physicals(struct inode *node)
{
	if(!node->physicals) {
		node->physicals = hash_table_create(0, 0, HASH_TYPE_CHAIN);
		hash_table_resize(node->physicals, HASH_RESIZE_MODE_IGNORE, 1000);
		hash_table_specify_function(node->physicals, HASH_FUNCTION_BYTE_SUM);
	}
}

addr_t fs_inode_map_shared_physical_page(struct inode *node, addr_t virt, 
		size_t offset, int flags)
{
	/* test if we have any shared mappings... */
	if(!node->physicals)
		return 0;
	int page_number = offset / PAGE_SIZE;
	void *value;
	if(hash_table_get_entry(node->physicals, &page_number, sizeof(page_number), 1, &value)
			== -ENOENT)
		return 0;
	struct physical_page *entry = value;
	assert(entry->count);
	/* so, we don't have to worry about someone decreasing to count to zero while we're working, 
	   since a process never calls this function without being responsible for one of the counts. */
	mutex_acquire(&entry->lock);
	if(!entry->page && (flags & FS_INODE_POPULATE))
	{
		/* map a new page into virt, and load data into it */
		entry->page = mm_alloc_physical_page();
		if(mm_vm_get_map(virt, 0, 0))
			panic(0, "trying to remap mminode section");
		mm_vm_map(virt, entry->page, PAGE_PRESENT | PAGE_WRITE | PAGE_USER, 0);
		int err;
		if((err=vfs_read_inode(node, offset, PAGE_SIZE, (void *)virt) < 0))
			printk(0, "[mminode]: read inode failed with %d\n", err);
	}
	addr_t ret = entry->page;
	mutex_release(&entry->lock);
	return ret;
}

static struct physical_page *__create_entry()
{
	struct physical_page *p = kmalloc(sizeof(struct physical_page));
	mutex_create(&p->lock, 0);
	return p;
}

void fs_inode_map_region(struct inode *node, size_t offset, size_t length)
{
	mutex_acquire(&node->mappings_lock);
	__init_physicals(node);
	assert(!(offset & ~PAGE_MASK));
	int page_number = offset / PAGE_SIZE;
	int npages = ((length-1) / PAGE_SIZE) + 1;
	for(int i=page_number;i<(page_number+npages);i++)
	{
		void *value;
		struct physical_page *entry;
		if(hash_table_get_entry(node->physicals, &i, sizeof(i), 1, &value) == -ENOENT)
		{
			entry = __create_entry();
			hash_table_set_entry(node->physicals, &i, sizeof(i), 1, entry);
		}
		else
			entry = value;

		/* bump the count... */
		mutex_acquire(&entry->lock);
		add_atomic(&entry->count, 1);
		mutex_release(&entry->lock);
	}
	mutex_release(&node->mappings_lock);
}

void fs_inode_sync_physical_page(struct inode *node, addr_t virt, size_t offset)
{
	assert(!(offset & ~PAGE_MASK));
	assert(!(virt & ~PAGE_MASK));
	if(vfs_write_inode(node, offset, PAGE_SIZE, (void *)virt) < 0)
		printk(0, "[mminode]: warning: failed to writeback data\n");
}

void fs_inode_unmap_region(struct inode *node, addr_t virt, size_t offset, size_t length)
{
	mutex_acquire(&node->mappings_lock);
	assert(node->physicals);
	assert(!(offset & ~PAGE_MASK));
	assert(!(virt & ~PAGE_MASK));
	int page_number = offset / PAGE_SIZE;
	int npages = ((length-1) / PAGE_SIZE) + 1;
	for(int i=page_number;i<(page_number+npages);i++)
	{
		void *value;
		struct physical_page *entry;
		if(hash_table_get_entry(node->physicals, &i, sizeof(i), 1, &value) == 0)
		{
			entry = value;
			mutex_acquire(&entry->lock);
			if(!sub_atomic(&entry->count, 1))
			{	
				fs_inode_sync_physical_page(node, virt + (i - page_number)*PAGE_SIZE, i * PAGE_SIZE);
				mm_free_physical_page(entry->page);
				entry->page = 0;
				mutex_destroy(&entry->lock);
				kfree(entry);
				hash_table_delete_entry(node->physicals, &i, sizeof(i), 1);
			} else
				mutex_release(&entry->lock);
			mm_vm_unmap_only(virt + (i - page_number)*PAGE_SIZE, 0);
		}
		/* and if the entry isn't found, we're all good! */
	}
	mutex_release(&node->mappings_lock);
}

void fs_inode_destroy_physicals(struct inode *node)
{
	/* this can only be called from free_inode, so we don't need to worry about locks */
	if(!node->physicals)
		return;
	assert(!node->physicals->count);
	hash_table_destroy(node->physicals);
	mutex_destroy(&node->mappings_lock);
}

