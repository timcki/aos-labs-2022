#include <types.h>
#include <list.h>
#include <paging.h>
#include <string.h>

#include <kernel/mem.h>

//#define BONUS_LAB1
#define FIND_BUDDY(p) pa2page(page2pa(p) ^ (1 << (p->pp_order)))

/* Physical page metadata. */
size_t npages;
struct page_info *pages;

/*
 * List of free buddy chunks (often also referred to as buddy pages or simply
 * pages). Each order has a list containing all free buddy chunks of the
 * specific buddy order. Buddy orders go from 0 to BUDDY_MAX_ORDER - 1
 */
struct list buddy_free_list[BUDDY_MAX_ORDER];

/* Counts the number of free pages for the given order.
 */
size_t count_free_pages(size_t order)
{
	struct list *node;
	size_t nfree_pages = 0;

	if (order >= BUDDY_MAX_ORDER) {
		return 0;
	}

	list_foreach(buddy_free_list + order, node) {
		++nfree_pages;
	}

	return nfree_pages;
}

/* Shows the number of free pages in the buddy allocator as well as the amount
 * of free memory in kiB.
 *
 * Use this function to diagnose your buddy allocator.
 */
void show_buddy_info(void)
{
	struct page_info *page;
	struct list *node;
	size_t order;
	size_t nfree_pages;
	size_t nfree = 0;

	cprintf("Buddy allocator:\n");

	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		nfree_pages = count_free_pages(order);

		cprintf("  order #%u pages=%u\n", order, nfree_pages);

		nfree += nfree_pages * (1 << (order + 12));
	}

	cprintf("  free: %u kiB\n", nfree / 1024);
}

/* Gets the total amount of free pages. */
size_t count_total_free_pages(void)
{
	struct page_info *page;
	struct list *node;
	size_t order;
	size_t nfree_pages;
	size_t nfree = 0;

	for (order = 0; order < BUDDY_MAX_ORDER; ++order) {
		nfree_pages = count_free_pages(order);
		nfree += nfree_pages * (order + 1);
	}

	return nfree;
}

/* Splits lhs into free pages until the order of the page is the requested
 * order req_order.
 *
 * The algorithm to split pages is as follows:
 *  - Given the page of order k, locate the page and its buddy at order k - 1.
 *  - Decrement the order of both the page and its buddy.
 *  - Mark the buddy page as free and add it to the free list.
 *  - Repeat until the page is of the requested order.
 *
 * Returns a page of the requested order.
 */
 struct page_info *buddy_split(struct page_info *lhs, size_t req_order)
{
        struct page_info *buddy;
        while(lhs->pp_order != req_order) {
		lhs->pp_order -= 1;
                buddy = pa2page(page2pa(lhs) ^ ((1 << (lhs->pp_order)) * PAGE_SIZE));
		buddy->pp_order = lhs->pp_order;
                buddy->pp_free = 1;
                // add the buddy to the list
                list_add(&buddy_free_list[buddy->pp_order], &(buddy->pp_node));
        }
		return lhs;
}

/* Merges the buddy of the page with the page if the buddy is free to form
 * larger and larger free pages until either the maximum order is reached or
 * no free buddy is found.
 *
 * The algorithm to merge pages is as follows:
 *  - Given the page of order k, locate the page with the lowest address
 *    and its buddy of order k.
 *  - Check if both the page and the buddy are free and whether the order
 *    matches.
 *  - Remove the page and its buddy from the free list.
 *  - Increment the order of the page.
 *  - Repeat until the maximum order has been reached or until the buddy is not
 *    free.
 *
 * Returns the largest merged free page possible.
 */
struct page_info *buddy_merge(struct page_info *page)
{
	// -> first find the buddy and the primary block
	/*struct page_info *primary = pa2page(page2pa(page) & (-1 << (1 + page->pp_order)));
	struct page_info *buddy = FIND_BUDDY(primary); // the buddy should have the same order as the primary

	while(primary->pp_free && buddy->pp_free && primary->pp_order < BUDDY_MAX_ORDER-1) {
		// the orders match, all free, order max not reached
		list_del(&(buddy->pp_node));
		list_del(&(primary->pp_node));

		primary->pp_order += 1;
		buddy = FIND_BUDDY(primary);
	}
	return primary;*/
	/***
	 * The previous method using primary blocks has some design flaws.
	 * The idea of this new method is:
	 * 	1. Find the physical address of the buddy memory of page.
	 * 	2. And then check the free list with order page -> pp_order.
	 * 	3. If there is a node having the same physical address with the desired buddy,
	 * 		we merge it with page.
	 * 	4. The memory with lower address will be the primary block.
	 ***/
	physaddr_t buddy_pa;// = page2pa(page) ^ ((1 << (page->pp_order)) * PAGE_SIZE);
        //list_add(&buddy_free_list[page->pp_order], &(page->pp_node));
        struct page_info *buddy, *temp_page;
        struct list *temp;

        while(page -> pp_order < BUDDY_MAX_ORDER) {
                buddy_pa = page2pa(page) ^ ((1 << (page->pp_order)) * PAGE_SIZE);
                buddy = NULL;

                list_foreach(buddy_free_list + page -> pp_order, temp) {
                        temp_page = container_of(temp, struct page_info, pp_node);
                        if(page2pa(temp_page) == buddy_pa) {
                                buddy = temp_page;
                                break;
                        }
                }

                if(buddy) {
                        list_del(&(buddy -> pp_node));
                        list_del(&(page -> pp_node));
                        page->pp_free = 0;
                        buddy->pp_free=0; // the two lines are required to pass the consistency test:)
                        page = (page2pa(page) < page2pa(buddy) ? page : buddy);
                        page -> pp_order += 1;
                        page -> pp_free = 1; // and here again, we set the 'primary' block to free.
                } else {
			break; // no merge possible
		}
        }
	// now the adding to free list is handled in this function, instead of in page_free.
        list_add(&buddy_free_list[page->pp_order], &(page->pp_node));
        return page;
}

/* Given the order req_order, attempts to find a page of that order or a larger
 * order in the free list. In case the order of the free page is larger than the
 * requested order, the page is split down to the requested order using
 * buddy_split().
 *
 * Returns a page of the requested order or NULL if no such page can be found.
 */
struct page_info *buddy_find(size_t req_order)
{
	size_t order = req_order;
	while(order < BUDDY_MAX_ORDER) {
		if(count_free_pages(order))
			break;
		order++;
	}
	if(order == BUDDY_MAX_ORDER)
		return NULL;
	struct page_info *page = container_of(list_pop_tail(buddy_free_list + order), struct page_info, pp_node);
	if(order > req_order) {
		page = buddy_split(page, req_order);
	}
	page -> pp_free = 0;
	return page;
}

/*
 * Allocates a physical page.
 *
 * if (alloc_flags & ALLOC_ZERO), fills the entire returned physical page with
 * '\0' bytes.
 * if (alloc_flags & ALLOC_HUGE), returns a huge physical 2M page.
 *
 * Beware: this function does NOT increment the reference count of the page -
 * this is the caller's responsibility.
 *
 * Returns NULL if out of free memory.
 *
 * Hint: use buddy_find() to find a free page of the right order.
 * Hint: use page2kva() and memset() to clear the page.
 */

struct page_info *page_alloc(int alloc_flags)
{
	struct page_info *page;
	size_t nbytes;
#ifdef BONUS_LAB1
	if (alloc_flags & ALLOC_HUGE) {
		page = buddy_find(9); // huge page order number
		nbytes = 2 * 1024 * 1024;
	} else {
		page = buddy_find(0); // one page
		nbytes = 4096;
		// use guard pages
		/*page = buddy_find(2); // 4 pages;
		nbytes = 4 * 4096;
		memset(page2kva(page), 0, nbytes);
		struct page_info *meta = page;
		meta -> pp_order -= 1;
		page = pa2page(page2pa(page) ^ ((1 << (page->pp_order)) * PAGE_SIZE));
		page -> pp_order = 0; //meta -> pp_order;
		page -> pp_free = 0;
		meta -> pp_free = 0;
		struct page_info *buddy = pa2page(page2pa(page) ^ ((1 << (page->pp_order)) * PAGE_SIZE));
		buddy -> pp_order = 0;
		buddy -> pp_free = 0;
		cprintf("%p %p %p\n", page2pa(meta), page2pa(page), page2pa(buddy));
		panic("!!!!!!!!!!!!");*/
	}
#else
	page = buddy_find(0);
	nbytes = 4096;
#endif
#ifdef BONUS_LAB1
	// zero the page to reduce the power of UAF
	// we were going to implement a random alloc alg, but since
	// the lack of random number generator support, we dropped this.
	// ( we even tried to get bios time using some asm, but there were some errors:(
	memset(page2kva(page), 0, nbytes);
#endif
	if (alloc_flags & ALLOC_ZERO) {
		memset(page2kva(page), 0, nbytes);
	}
	//page -> pp_free = 0;
	return page;
}

/*
 * Return a page to the free list.
 * (This function should only be called when pp->pp_ref reaches 0.)
 *
 * Hint: mark the page as free and use buddy_merge() to merge the free page
 * with its buddies before returning the page to the free list.
 */
void page_free(struct page_info *pp)
{
	assert(pp->pp_ref == 0);
#ifdef BONUS_LAB1
	// check invalid free
	struct page_info *primary = pa2page( page2pa(pp) & ( ((long)-1 << (1 + pp->pp_order) ) * PAGE_SIZE) );
	if(page2pa(pp) % PAGE_SIZE != 0) {
		cprintf("Trying to free an invalid page\n");
	}
	if(page2pa(primary) == page2pa(pp)) {
		// if pp is the primary block
		// seems like not much we can do?
	} else {
		// the buddy is the primary block
		assert((page2pa(pp) ^ ((1 << (pp->pp_order)) * PAGE_SIZE)) == page2pa(primary));
		if(primary -> pp_order > pp -> pp_order) {
			// which means that pp is not valid
			cprintf("invalid free detected\n");
			return;
		}
	}
	if(pp -> pp_free == 1) {
                // double free
                cprintf("double free detected at page %p\n", page2pa(pp));
                //return;
        }
#endif
	pp->pp_free = 1;
#ifdef BONUS_LAB1
	/*if(pp -> pp_order == 0) {
		// check and recover
		// buggy!
		struct page_info *buddy = pa2page(page2pa(pp) ^ ((1 << (pp->pp_order)) * PAGE_SIZE));
		for(int i = 0; i < 4096; i++) {
			if(page2pa(buddy) + i) {
				//cprintf("out of bounds write detected\n");
			}
		}
		pp -> pp_order = 1;
		buddy = pa2page(page2pa(pp) ^ ((1 << (pp->pp_order)) * PAGE_SIZE));
		for(int i = 0; i < 2 * 4096; i++) {
                        if(page2pa(buddy) + i) {
                                //cprintf("out of bounds write detected\n");
                        }
                }
		buddy -> pp_order = 2;
		pp = buddy;
	}*/
#endif
	struct page_info *merged = buddy_merge(pp);
	//cprintf("adding free: %p\n", page2pa(merged));
	
	//list_add(&buddy_free_list[merged->pp_order], &(merged->pp_node));
}

/*
 * Decrement the reference count on a page,
 * freeing it if there are no more refs.
 */
void page_decref(struct page_info *pp)
{
	if (--pp->pp_ref == 0) {
		page_free(pp);
	}
}
