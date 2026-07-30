/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 *
 * Boot-time physical page management backed by the common page_t.  The
 * RAM above a small boot-allocator reserve is described by a real
 * page_t array and served from a freelist, so the tested page-table
 * engine draws its translation tables from page_t allocation rather
 * than bootmem, keeps a per-page HAT mapping list of hments and
 * maintains the common P_REF/P_MOD/P_RO page attributes through the
 * common hat_page_setattr()/hat_page_getattr()/hat_page_clrattr()
 * entry points.
 *
 * Mapping operations run on the boot CPU during the bootstrap, so the
 * freelist and mapping lists are unlocked here; the full common HAT
 * adds p_selock and hat-layer lock discipline at the next boundary.
 */

#include <sys/types.h>
#include <sys/controlregs.h>
#include <sys/machparam.h>
#include <sys/bootmem.h>
#include <sys/boot_hat.h>
#include <sys/boot_page.h>
#include <vm/page.h>
#include <vm/hat.h>

/* Boot allocations that still land in bootmem after the arena is carved. */
#define	ARM64_BOOTMEM_RESERVE	(4ULL * 1024 * 1024)
#define	HMENTS_PER_PAGE		(MMU_PAGESIZE / sizeof (struct arm64_hment))
#define	PAGE_ATTR_MASK		(P_MOD | P_REF | P_RO)
#define	MPIDR_AFFINITY_MASK	0xff00ffffffULL

extern void pl011_puts(const char *);

static page_t *page_array;
static pfn_t page_base_pfn;
static uint64_t page_pages;
static uint64_t page_arena_base;
static uint64_t page_freecnt;
static page_t *page_freelist;
static struct arm64_hment *hment_freelist;
static uint64_t page_boot_mpidr;
static int page_ready;

/*
 * Single-threaded boot contract.  The page freelist, the hment pool and
 * the per-page mapping lists carry no locks during the bootstrap: every
 * allocation, free and mapping-list update is required to run on the
 * boot CPU while the secondaries are parked or spinning in the SGI
 * cross-call handler.  p_selock and the hat-layer lock discipline arrive
 * with the full common HAT at the next integration boundary.
 *
 * Until then, catch a violation loudly at each entry point instead of
 * silently corrupting a list.  arm64_page_init() records the boot CPU's
 * MPIDR affinity; a single system-register read per call is cheap enough
 * for the boot path and works before arm64_smp_start() has built the
 * logical CPU index that arm64_smp_current_cpuid() depends on.
 */
static int
page_on_boot_cpu(const char *who)
{
	if (page_ready == 0)
		return (1);
	if ((read_sysreg(mpidr_el1) & MPIDR_AFFINITY_MASK) == page_boot_mpidr)
		return (1);
	pl011_puts("boot_page: ");
	pl011_puts(who);
	pl011_puts(" called off the boot CPU\n");
	return (0);
}

int
arm64_page_initialized(void)
{
	return (page_ready);
}

pfn_t
page_pptonum(page_t *pp)
{
	return (pp->p_pagenum);
}

page_t *
page_numtopp_nolock(pfn_t pfn)
{
	if (page_array == 0 || pfn < page_base_pfn ||
	    pfn >= page_base_pfn + page_pages)
		return (0);
	return (&page_array[pfn - page_base_pfn]);
}

uint64_t
arm64_page_pa(const struct page *pp)
{
	return ((uint64_t)pp->p_pagenum << MMU_PAGESHIFT);
}

uint64_t
arm64_page_total(void)
{
	return (page_pages);
}

uint64_t
arm64_page_freecnt(void)
{
	return (page_freecnt);
}

void
hat_page_setattr(struct page *pp, uint_t flag)
{
	pp->p_nrm |= (uchar_t)(flag & PAGE_ATTR_MASK);
}

void
hat_page_clrattr(struct page *pp, uint_t flag)
{
	pp->p_nrm &= (uchar_t)~(flag & PAGE_ATTR_MASK);
}

uint_t
hat_page_getattr(struct page *pp, uint_t flag)
{
	return (pp->p_nrm & flag);
}

static void
page_freelist_push(page_t *pp)
{
	pp->p_next = page_freelist;
	page_freelist = pp;
	PP_SETFREE(pp);
	page_freecnt++;
}

int
arm64_page_init(const struct arm64_platform *platform, uint64_t fdt,
    uint64_t fdt_size)
{
	uint64_t ram_end, pa, fdt_start, fdt_end, i;
	page_t *pp;

	if (page_ready != 0 || platform == 0 || platform->ram_size == 0)
		return (-1);
	ram_end = platform->ram_base + platform->ram_size;
	page_base_pfn = (pfn_t)(platform->ram_base >> MMU_PAGESHIFT);
	page_pages = platform->ram_size >> MMU_PAGESHIFT;
	page_array = arm64_bootmem_alloc(page_pages * sizeof (page_t),
	    sizeof (uint64_t));
	if (page_array == 0)
		return (-1);
	page_arena_base = arm64_bootmem_carve_tail(ARM64_BOOTMEM_RESERVE,
	    MMU_PAGESIZE);
	if (page_arena_base == 0 || page_arena_base >= ram_end)
		return (-1);
	fdt_start = fdt & ~(uint64_t)MMU_PAGEOFFSET;
	fdt_end = (fdt + fdt_size + MMU_PAGEOFFSET) &
	    ~(uint64_t)MMU_PAGEOFFSET;
	for (i = 0; i != page_pages; i++) {
		pp = &page_array[i];
		pp->p_pagenum = page_base_pfn + i;
		pa = (uint64_t)pp->p_pagenum << MMU_PAGESHIFT;
		if (pa < page_arena_base ||
		    (pa < fdt_end && pa + MMU_PAGESIZE > fdt_start)) {
			PP_SETBOOTPAGES(pp);
			continue;
		}
		page_freelist_push(pp);
	}
	if (page_freecnt == 0)
		return (-1);
	page_boot_mpidr = read_sysreg(mpidr_el1) & MPIDR_AFFINITY_MASK;
	page_ready = 1;
	return (0);
}

/* Boot-CPU only; see the single-threaded boot contract above. */
page_t *
arm64_page_alloc(void)
{
	page_t *pp = page_freelist;
	uint64_t *mem;
	uint64_t i;

	if (page_ready == 0 || pp == 0)
		return (0);
	if (page_on_boot_cpu("arm64_page_alloc") == 0)
		return (0);
	page_freelist = pp->p_next;
	pp->p_next = 0;
	PP_CLRFREE(pp);
	page_freecnt--;
	mem = (uint64_t *)(uintptr_t)arm64_page_pa(pp);
	for (i = 0; i != MMU_PAGESIZE / sizeof (uint64_t); i++)
		mem[i] = 0;
	return (pp);
}

/* Boot-CPU only; see the single-threaded boot contract above. */
int
arm64_page_free(struct page *pp)
{
	if (page_on_boot_cpu("arm64_page_free") == 0)
		return (-1);
	if (page_ready == 0 || pp == 0 || PP_ISFREE(pp) ||
	    PP_ISBOOTPAGES(pp) || pp->p_mapping != 0 || pp->p_share != 0)
		return (-1);
	pp->p_nrm = 0;
	page_freelist_push(pp);
	return (0);
}

static struct arm64_hment *
hment_alloc(void)
{
	struct arm64_hment *hm;
	page_t *pp;
	uint64_t i;

	if (hment_freelist == 0) {
		pp = arm64_page_alloc();
		if (pp == 0)
			return (0);
		hm = (struct arm64_hment *)(uintptr_t)arm64_page_pa(pp);
		for (i = 0; i != HMENTS_PER_PAGE; i++) {
			hm[i].hm_next = hment_freelist;
			hment_freelist = &hm[i];
		}
	}
	hm = hment_freelist;
	hment_freelist = hm->hm_next;
	hm->hm_next = 0;
	return (hm);
}

/*
 * Record one translation of a physical page.  Physical addresses with
 * no page_t (device MMIO) and calls before initialization are accepted
 * silently so the page-table engine keeps working across the boundary.
 *
 * Boot-CPU only; see the single-threaded boot contract above.
 *
 * Establishing a translation only marks the page referenced.  P_MOD in
 * the common contract means the page really was modified, so it is left
 * to the dirty-bit path rather than being asserted here for every
 * writable mapping.
 */
int
arm64_page_mapping_add(uint64_t physical, uint64_t virtual, uint16_t asid,
    uint_t flags)
{
	page_t *pp;
	struct arm64_hment *hm;

	if (page_ready == 0)
		return (0);
	if (page_on_boot_cpu("arm64_page_mapping_add") == 0)
		return (-1);
	pp = page_numtopp_nolock((pfn_t)(physical >> MMU_PAGESHIFT));
	if (pp == 0)
		return (0);
	hm = hment_alloc();
	if (hm == 0)
		return (-1);
	hm->hm_va = virtual;
	hm->hm_asid = asid;
	hm->hm_next = (struct arm64_hment *)pp->p_mapping;
	pp->p_mapping = hm;
	pp->p_share++;
	hat_page_setattr(pp, P_REF);
	return (0);
}

/* Boot-CPU only; see the single-threaded boot contract above. */
int
arm64_page_mapping_remove(uint64_t physical, uint64_t virtual, uint16_t asid)
{
	page_t *pp;
	struct arm64_hment *hm, *walk;

	if (page_ready == 0)
		return (0);
	if (page_on_boot_cpu("arm64_page_mapping_remove") == 0)
		return (-1);
	pp = page_numtopp_nolock((pfn_t)(physical >> MMU_PAGESHIFT));
	if (pp == 0)
		return (0);
	hm = (struct arm64_hment *)pp->p_mapping;
	if (hm != 0 && hm->hm_va == virtual && hm->hm_asid == asid) {
		pp->p_mapping = hm->hm_next;
	} else {
		walk = hm;
		hm = 0;
		while (walk != 0 && walk->hm_next != 0) {
			if (walk->hm_next->hm_va == virtual &&
			    walk->hm_next->hm_asid == asid) {
				hm = walk->hm_next;
				walk->hm_next = hm->hm_next;
				break;
			}
			walk = walk->hm_next;
		}
	}
	if (hm == 0)
		return (-1);
	hm->hm_next = hment_freelist;
	hment_freelist = hm;
	pp->p_share--;
	return (0);
}

struct arm64_hment *
arm64_page_mapping_find(uint64_t physical, uint64_t virtual, uint16_t asid)
{
	page_t *pp;
	struct arm64_hment *hm;

	pp = page_numtopp_nolock((pfn_t)(physical >> MMU_PAGESHIFT));
	if (pp == 0)
		return (0);
	for (hm = (struct arm64_hment *)pp->p_mapping; hm != 0;
	    hm = hm->hm_next) {
		if (hm->hm_va == virtual && hm->hm_asid == asid)
			return (hm);
	}
	return (0);
}
