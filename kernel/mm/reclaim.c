#include <sea/mm/kmalloc.h>
#include <sea/mm/reclaim.h>
#include <sea/ll.h>

struct llist reclaimers;

void mm_reclaim_init(void)
{
	ll_create(&reclaimers, 0);
}

void mm_reclaim_register(size_t (*fn)(void), size_t size)
{
	struct reclaimer *rec = kmalloc(sizeof(struct reclaimer));
	rec->size = size;
	rec->fn = fn;
	ll_insert(&reclaimers, rec);
}

void mm_reclaim_size(size_t size)
{
	size_t amount = 0, thisround = 1;
	while(amount < size && thisround != 0) {
		struct llistnode *node;
		struct reclaimer *rec;
		thisround = 0;
		rwlock_acquire(&reclaimers->rwl, RWL_READER);
		ll_for_each_entry(&reclaimers, node, struct reclaimer *, rec) {
			size_t freed = rec->fn();
			amount += freed;
			thisround += freed;
			if(amount > size)
				break;
		}
		rwlock_release(&reclaimers->rwl, RWL_READER);
	}
}

void mm_reclaim(void)
{
	rwlock_acquire(&reclaimers->rwl, RWL_READER);
	ll_for_each_entry(&reclaimers, node, struct reclaimer *, rec) {
		if(rec->fn())
			break;
	}
	rwlock_release(&reclaimers->rwl, RWL_READER);
}

