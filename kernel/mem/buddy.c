#include <types.h>
#include <list.h>
#include <paging.h>
#include <string.h>

#include <kernel/mem.h>

#define FIND_BUDDY(p) pa2page(page2pa(p) ^ ((1 << (lhs->pp_order)) * PAGE_SIZE))
#define FIND_PRIMARY(p) pa2page(page2pa(p) & (((long)-1 << (1 + p->pp_order)) * PAGE_SIZE))

/* Physical page metadata. */
size_t npages;
struct page_info *pages;

/*
 * List of free buddy chunks (often also referred to as buddy pages or simply
 * pages). Each order has a list containing all free buddy chunks of the
 * specific buddy order. Buddy orders go from 0 to BUDDY_MAX_ORDER - 1
 */
struct list buddy_free_list[BUDDY_MAX_ORDER];

// Counts the number of free pages for the given order.
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
		nfree += nfree_pages * (1 << order);
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
			buddy = FIND_BUDDY(lhs);
			buddy->pp_order = lhs->pp_order;
			buddy->pp_free = 1;
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
	/*
	 * The previous method using primary blocks has some design flaws.
	 * The idea of this new method is:
	 * 	1. Find the physical address of the buddy memory of page.
	 * 	2. And then check the free list with order page -> pp_order.
	 * 	3. If there is a node having the same physical address with the desired buddy,
	 * 		we merge it with page.
	 * 	4. The memory with lower address will be the primary block.
	 */
	struct page_info *buddy;
	struct list *temp;

	do {
		physaddr_t buddy_pa = page2pa(page) ^ ((1 << (page->pp_order)) * PAGE_SIZE);
		buddy = NULL;

		list_foreach(&buddy_free_list[page->pp_order], temp) {
			struct page_info *tb = container_of(temp, struct page_info, pp_node);
			if (page2pa(tb) == buddy_pa) {
					buddy = tb;
					break;
			}
		}

		if (buddy) {
			list_del(&(buddy->pp_node));
			list_del(&(page->pp_node));
			page->pp_free = 0;
			buddy->pp_free = 0; // the two lines are required to pass the consistency test:)
			page = (page2pa(page) < page2pa(buddy) ? page : buddy);
			page->pp_order += 1;
			page->pp_free = 1; // and here again, we set the 'primary' block to free.
		}
	} while (page->pp_order < BUDDY_MAX_ORDER && buddy != NULL);

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
	while (order < BUDDY_MAX_ORDER) {
		if (count_free_pages(order)) break;
		order++;
	}
	if (order == BUDDY_MAX_ORDER)
		return NULL;
	struct page_info *page = container_of(list_pop_tail(buddy_free_list + order), struct page_info, pp_node);
	if (order > req_order) {
		page = buddy_split(page, req_order);
	}
	page->pp_free = 0;
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
	}
#else
	page = buddy_find(0);
	nbytes = 4096;
#endif
#ifdef BONUS_LAB1
	// zero the page to reduce the power of UAF
	// we were going to implement a random alloc alg, but since
	// the lack of random number generator support, we dropped this.
	// (we even tried to get bios time using some asm, but there were errors)
	memset(page2kva(page), 0, nbytes);
#endif
	if (alloc_flags & ALLOC_ZERO)
		memset(page2kva(page), 0, nbytes);
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
	struct page_info *primary = FIND_PRIMARY(pp);
	if (page2pa(pp) % PAGE_SIZE)
		cprintf("Trying to free an invalid page\n");

	if (page2pa(primary) == page2pa(pp)) {
		// if pp is the primary block
		// seems like not much we can do?
	} else {
		// the buddy is the primary block
		assert((page2pa(pp) ^ ((1 << (pp->pp_order)) * PAGE_SIZE)) == page2pa(primary));
		if (primary->pp_order > pp->pp_order) {
			// which means that pp is not valid
			cprintf("invalid free detected\n");
			return;
		}
	}

	// double free
	if(pp->pp_free)
		cprintf("double free detected at page %p\n", page2pa(pp));
#endif
	pp->pp_free = 1;
	struct page_info *merged = buddy_merge(pp);
	
	list_add(&buddy_free_list[merged->pp_order], &(merged->pp_node));
}

/*
 * Decrement the reference count on a page,
 * freeing it if there are no more refs.
 */
void page_decref(struct page_info *pp)
{
	if (--pp->pp_ref == 0) 
		page_free(pp);
}

static int in_page_range(void *p)
{
	return ((uintptr_t)pages <= (uintptr_t)p &&
	        (uintptr_t)p < (uintptr_t)(pages + npages));
}

static void *update_ptr(void *p)
{
	if (!in_page_range(p))
		return p;

	return (void *)((uintptr_t)p + KPAGES - (uintptr_t)pages);
}

void buddy_migrate(void)
{
	struct page_info *page;
	struct list *node;
	size_t i;

	for (i = 0; i < npages; ++i) {
		page = pages + i;
		node = &page->pp_node;

		node->next = update_ptr(node->next);
		node->prev = update_ptr(node->prev);
	}

	for (i = 0; i < BUDDY_MAX_ORDER; ++i) {
		node = buddy_free_list + i;

		node->next = update_ptr(node->next);
		node->prev = update_ptr(node->prev);
	}

	pages = (struct page_info *)KPAGES;
}

int buddy_map_chunk(struct page_table *pml4, size_t index)
{
	struct page_info *page, *base;
	void *end;
	size_t nblocks = (1 << (12 + BUDDY_MAX_ORDER - 1)) / PAGE_SIZE;
	size_t nalloc = ROUNDUP(nblocks * sizeof *page, PAGE_SIZE) / PAGE_SIZE;
	size_t i;

	index = ROUNDDOWN(index, nblocks);
	base = pages + index;

	for (i = 0; i < nalloc; ++i) {
		page = page_alloc(ALLOC_ZERO);

		if (!page) {
			return -1;
		}

		if (page_insert(pml4, page, (char *)base + i * PAGE_SIZE,
		    PAGE_PRESENT | PAGE_WRITE | PAGE_NO_EXEC) < 0) {
			return -1;
		}
	}

	for (i = 0; i < nblocks; ++i) {
		page = base + i;
		list_init(&page->pp_node);
	}

	npages = index + nblocks;

	return 0;
}

