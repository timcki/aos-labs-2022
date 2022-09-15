#include <types.h>
#include <paging.h>

#include <kernel/mem.h>

struct boot_map_info {
	struct page_table *pml4;
	uint64_t flags;
	physaddr_t pa;
	uintptr_t base, end;
};

/* Stores the physical address and the appropriate permissions into the PTE and
 * increments the physical address to point to the next page.
 */
static int boot_map_pte(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct boot_map_info *info = walker->udata;

	/* LAB 2: your code here. i*/
	// start
	*entry = info -> pa | info -> flags;
	(info -> pa) += PAGE_SIZE;
	// end
	return 0;
}

/* Stores the physical address and the appropriate permissions into the PDE and
 * increments the physical address to point to the next huge page if the
 * physical address is huge page aligned and if the area to be mapped covers a
 * 2M area. Otherwise this function calls ptbl_split() to split down the huge
 * page or allocate a page table.
 */
static int boot_map_pde(physaddr_t *entry, uintptr_t base, uintptr_t end,
    struct page_walker *walker)
{
	struct boot_map_info *info = walker->udata;

	/* LAB 2: your code here. */
	// start
	*entry = info -> pa | info -> flags;
	if(info -> flags & PAGE_HUGE) {
		(info -> pa) += PAGE_SIZE * 512;
	} else {
		ptbl_split(entry, base, end, walker); // return value is ignored here.
	}
	// end
	return 0;
}

/*
 * Maps the virtual address space at [va, va + size) to the contiguous physical
 * address space at [pa, pa + size). Size is a multiple of PAGE_SIZE. The
 * permissions of the page to set are passed through the flags argument.
 *
 * This function is only intended to set up static mappings. As such, it should
 * not change the reference counts of the mapped pages.
 *
 * Hint: this function calls walk_page_range().
 */
void boot_map_region(struct page_table *pml4, void *va, size_t size,
    physaddr_t pa, uint64_t flags)
{
	/* LAB 2: your code here. */
	struct boot_map_info info = {
		.pa = pa,
		.flags = flags,
		.base = ROUNDDOWN((uintptr_t)va, PAGE_SIZE),
		.end = ROUNDUP((uintptr_t)va + size, PAGE_SIZE) - 1,
	};
	struct page_walker walker = {
		.pte_callback = boot_map_pte,
		.pde_callback = boot_map_pde,
		/* LAB 2: your code here. */
		// TODO: do we need add other callbacks here? --- Yiming
		.udata = &info,
	};

	walk_page_range(pml4, va, (void *)((uintptr_t)va + size), &walker);
}

/* This function parses the program headers of the ELF header of the kernel
 * to map the regions into the page table with the appropriate permissions.
 *
 * First creates an identity mapping at the KERNEL_VMA of size BOOT_MAP_LIM
 * with permissions RW-.
 *
 * Then iterates the program headers to map the regions with the appropriate
 * permissions.
 *
 * Hint: this function calls boot_map_region().
 * Hint: this function ignores program headers below KERNEL_VMA (e.g. ".boot").
 */
void boot_map_kernel(struct page_table *pml4, struct elf *elf_hdr)
{
	struct elf_proghdr *prog_hdr =
	    (struct elf_proghdr *)((char *)elf_hdr + elf_hdr->e_phoff);
	uint64_t flags;
	size_t i;
	struct elf_proghdr *cur_hdr;
	uintptr_t va;

	/* LAB 2: your code here. */
	// start
	boot_map_region(pml4, KERNEL_VMA, BOOT_MAP_LIM, 0x100000, PAGE_WRITE | PAGE_PRESENT | PAGE_NO_EXEC); // didn't find PAGE_READ
	for(i = 0; i < elf_hdr -> e_phnum; i++) {
		cur_hdr = prog_hdr + i;
		va = cur_hdr -> p_va;
		if(cur_hdr -> p_type == ELF_PROG_LOAD && va >= KERNEL_VMA) {
			flags = 0;
			if(!(cur_hdr -> p_flags & ELF_PROG_FLAG_EXEC)) {
				flags |= PAGE_NO_EXEC;
			}
			if(cur_hdr -> p_flags & ELF_PROG_FLAG_WRITE) {
				flags |= PAGE_WRITE;
			}
			boot_map_region(pml4, va, cur_hdr -> p_memsz, cur_hdr -> p_pa, flags);
		}
	}
	// end
}
