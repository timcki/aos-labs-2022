#include <types.h>
#include <string.h>
#include <paging.h>

#include <kernel/mem.h>

/* Allocates a page table if none is present for the given entry.
 * If there is already something present in the PTE, then this function simply
 * returns. Otherwise, this function allocates a page using page_alloc(),
 * increments the reference count and stores the newly allocated page table
 * with the PAGE_PRESENT | PAGE_WRITE | PAGE_USER permissions.
 */
int ptbl_alloc(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	/* LAB 2: your code here. */
	// start
	if(*entry & PAGE_PRESENT)
		return 0;
	struct page_info *page = page_alloc(0);
	(page -> pp_ref) += 1;
	*entry = page2pa(page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	// end
	return 0;
}

/* Splits up a huge page by allocating a new page table and setting up the huge
 * page into smaller pages that consecutively make up the huge page.
 *
 * If no huge page was mapped at the entry, simply allocate a page table.
 *
 * Otherwise if a huge page is present, allocate a new page, increment the
 * reference count and have the PDE point to the newly allocated page. This
 * page is used as the page table. Then allocate a normal page for each entry,
 * copy over the data from the huge page and set each PDE.
 *
 * Hint: the only user of this function is boot_map_region(). Otherwise the 2M
 * physical page has to be split down into its individual 4K pages by updating
 * the respective struct page_info structs.
 *
 * Hint: this function calls ptbl_alloc(), page_alloc(), page2pa() and
 * page2kva().
 */
int ptbl_split(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	/* LAB 2: your code here. */
	// start
	int res;
	if(!(*entry & PAGE_HUGE)) {
		res = ptbl_alloc(entry, base, end, walker);
		return res;
	}
	struct page_info *page = page_alloc(0);
	(page -> pp_ref) += 1;
	*entry = page2pa(page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	physaddr_t *temp;
	for(size_t i = 0; i < PAGE_SIZE / sizeof(physaddr_t); i++) {
		// for each entry in the PDE (i.e., the variable entry)
		// seems like we need to transform the pa to kva here?
		temp = (physaddr_t *)((physaddr_t)page2kva(page) + 8);
		res = ptbl_alloc(temp, base, end, walker);
	}
	return res;
	// end
	return 0;
}

/* Attempts to merge all consecutive pages in a page table into a huge page.
 *
 * First checks if the PDE points to a huge page. If the PDE points to a huge
 * page there is nothing to do. Otherwise the PDE points to a page table.
 * Then this function checks all entries in the page table to check if they
 * point to present pages and share the same flags. If not all pages are
 * present or if not all flags are the same, this function simply returns.
 * At this point the pages can be merged into a huge page. This function now
 * allocates a huge page and copies over the data from the consecutive pages
 * over to the huge page.
 * Finally, it sets the PDE to point to the huge page with the flags shared
 * between the previous pages.
 *
 * Hint: don't forget to free the page table and the previously used pages.
 */
int ptbl_merge(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	/* LAB 2: your code here. */
	// start
	int res;
	if(*entry & PAGE_HUGE) {
		return 0;
	}
	if(!(*entry & PAGE_PRESENT)) {
		return 0;
	}
	for(size_t i = 0; i < PAGE_SIZE / sizeof(physaddr_t); i++) {
		if(!(*(physaddr_t *)(entry[i]) & PAGE_PRESENT)) { // TODO: missing the same flags condition: where to find these flags? in udata?
			return 0;
		}
	}
	struct page_info *page = page_alloc(ALLOC_HUGE);
	(page -> pp_ref) += 1;
        *entry = page2pa(page) | PAGE_PRESENT | PAGE_WRITE | PAGE_USER | PAGE_HUGE;
	// TODO: how to set the flags
	// free previously used pages
	for(size_t i = 0; i < PAGE_SIZE / sizeof(physaddr_t); i++) {
		if(!(*(physaddr_t *)(entry[i]) & PAGE_PRESENT)) {
			page_free(pa2page(PAGE_ADDR(*(physaddr_t *)(entry[i]))));
		}
	}
	// end
	return 0;
}

/* Frees up the page table by checking if all entries are clear. Returns if no
 * page table is present. Otherwise this function checks every entry in the
 * page table and frees the page table if no entry is set.
 *
 * Hint: this function calls pa2page(), page2kva() and page_free().
 */
int ptbl_free(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	/* LAB 2: your code here. */
	// start
	if(!(*entry & PAGE_PRESENT)) {
		return 0;
	}
	for(size_t i = 0; i < PAGE_SIZE / sizeof(physaddr_t); i++) {
                if(*(physaddr_t *)(entry[i]) & PAGE_PRESENT) {
                        return 0;
                }
        }
	page_free(pa2page(PAGE_ADDR(*entry)));
	// end
	return 0;
}
