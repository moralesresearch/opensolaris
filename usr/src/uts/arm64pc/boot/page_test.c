/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/machparam.h>
#include <sys/boot_hat.h>
#include <sys/boot_page.h>
#include <vm/page.h>
#include <vm/hat.h>

#define	PAGE_TEST_VA		(KERNELBASE + 0x500000000ULL)
#define	PAGE_TEST_ALIAS1_VA	(PAGE_TEST_VA + 0x1000ULL)
#define	PAGE_TEST_ALIAS2_VA	(PAGE_TEST_VA + 0x2000ULL)
#define	PAGE_TEST_BULK_VA	(PAGE_TEST_VA + 0x100000ULL)
#define	PAGE_TEST_REMAP_VA	(PAGE_TEST_VA + 0x200000ULL)
#define	PAGE_TEST_USER_VA	0x0000000600000000ULL
#define	PAGE_TEST_ASID		9
#define	PAGE_TEST_PATTERN	0xa5a55a5a12345678ULL
#define	PAGE_TEST_DIRTY		0xa5a55a5a12345679ULL

/*
 * One hment page holds MMU_PAGESIZE / sizeof (struct arm64_hment)
 * entries, which is 170 at the current layout.  Taking more mappings
 * than that in one go forces hment_alloc() through its pool-growth path.
 */
#define	PAGE_TEST_BULK		200U

/*
 * Exercise the common page_t freelist, the page_t-backed translation
 * tables, the per-page HAT mapping lists and the common page-attribute
 * entry points end to end against the tested page-table engine.
 */
int
arm64_page_selftest(void)
{
	page_t *pp, *upp, *old_pp, *new_pp;
	struct arm64_hment *hm;
	uint64_t free_before, tables_before, pa, upa, translated;
	uint64_t old_pa, new_pa, i;
	volatile uint64_t *mem;
	volatile uint64_t *alias = (volatile uint64_t *)(uintptr_t)
	    PAGE_TEST_VA;
	struct arm64_boot_as as;
	uint64_t old_ttbr0;

	if (arm64_page_initialized() == 0 || arm64_page_freecnt() == 0 ||
	    arm64_page_freecnt() > arm64_page_total())
		return (-1);

	/* page_t freelist allocation and pfn round trips. */
	free_before = arm64_page_freecnt();
	pp = arm64_page_alloc();
	if (pp == 0 || PP_ISFREE(pp) ||
	    arm64_page_freecnt() != free_before - 1)
		return (-1);
	pa = arm64_page_pa(pp);
	if (page_pptonum(pp) != (pfn_t)(pa >> MMU_PAGESHIFT) ||
	    page_numtopp_nolock(page_pptonum(pp)) != pp)
		return (-1);
	mem = (volatile uint64_t *)(uintptr_t)pa;
	if (*mem != 0)
		return (-1);
	*mem = PAGE_TEST_PATTERN;

	/* Kernel mapping builds its tables from the page_t freelist. */
	tables_before = arm64_boot_hat_table_pages();
	if (arm64_boot_hat_map(PAGE_TEST_VA, pa, ARM64_HAT_WRITE) != 0 ||
	    arm64_boot_hat_table_pages() <= tables_before ||
	    arm64_boot_hat_lookup(PAGE_TEST_VA, &translated) != 0 ||
	    translated != pa || *alias != PAGE_TEST_PATTERN)
		return (-1);

	/* The mapping is recorded on the page's HAT mapping list. */
	hm = arm64_page_mapping_find(pa, PAGE_TEST_VA, 0);
	if (hm == 0 || hm->hm_va != PAGE_TEST_VA || hm->hm_asid != 0 ||
	    pp->p_share != 1 || pp->p_mapping == 0)
		return (-1);

	/*
	 * Common page attributes via the common HAT entry points.
	 * Establishing a writable mapping only marks the page referenced:
	 * P_MOD means the page really was modified, so it must still be
	 * clear here.
	 */
	if (hat_page_getattr(pp, P_REF) == 0 ||
	    hat_page_getattr(pp, P_MOD) != 0)
		return (-1);
	/*
	 * Write through the mapping and record the dirty bit explicitly.
	 * The hardware dirty-bit / write-fault path that will set P_MOD on
	 * its own is deferred to the as_fault() work at the next
	 * integration boundary; until then this stands in for it.
	 */
	*alias = PAGE_TEST_DIRTY;
	hat_page_setattr(pp, P_MOD);
	if (*mem != PAGE_TEST_DIRTY || hat_page_getattr(pp, P_MOD) == 0)
		return (-1);
	hat_page_clrattr(pp, P_REF | P_MOD);
	if (hat_page_getattr(pp, P_REF | P_MOD | P_RO) != 0)
		return (-1);
	hat_page_setattr(pp, P_RO);
	if (hat_page_getattr(pp, P_RO) == 0 ||
	    hat_page_getattr(pp, P_REF | P_MOD) != 0)
		return (-1);
	hat_page_clrattr(pp, P_RO);

	/* Unmap removes the hment; a mapped or shared page cannot free. */
	if (arm64_page_free(pp) == 0)
		return (-1);
	if (arm64_boot_hat_unmap(PAGE_TEST_VA) != 0 ||
	    pp->p_share != 0 || pp->p_mapping != 0 ||
	    arm64_page_mapping_find(pa, PAGE_TEST_VA, 0) != 0)
		return (-1);

	/*
	 * Several translations of one physical page.  The list is built
	 * head first, so removing the middle mapping exercises the
	 * interior-unlink walk in arm64_page_mapping_remove().
	 */
	if (arm64_boot_hat_map(PAGE_TEST_VA, pa, ARM64_HAT_WRITE) != 0 ||
	    arm64_boot_hat_map(PAGE_TEST_ALIAS1_VA, pa,
	    ARM64_HAT_WRITE) != 0 ||
	    arm64_boot_hat_map(PAGE_TEST_ALIAS2_VA, pa,
	    ARM64_HAT_WRITE) != 0 || pp->p_share != 3)
		return (-1);
	if (*(volatile uint64_t *)(uintptr_t)PAGE_TEST_ALIAS1_VA !=
	    PAGE_TEST_DIRTY ||
	    *(volatile uint64_t *)(uintptr_t)PAGE_TEST_ALIAS2_VA !=
	    PAGE_TEST_DIRTY)
		return (-1);
	if (arm64_page_mapping_remove(pa, PAGE_TEST_ALIAS1_VA, 0) != 0 ||
	    pp->p_share != 2 ||
	    arm64_page_mapping_find(pa, PAGE_TEST_ALIAS1_VA, 0) != 0 ||
	    arm64_page_mapping_find(pa, PAGE_TEST_VA, 0) == 0 ||
	    arm64_page_mapping_find(pa, PAGE_TEST_ALIAS2_VA, 0) == 0)
		return (-1);
	/* A second removal has nothing left to unlink. */
	if (arm64_page_mapping_remove(pa, PAGE_TEST_ALIAS1_VA, 0) == 0)
		return (-1);
	if (arm64_boot_hat_unmap(PAGE_TEST_ALIAS1_VA) != 0 ||
	    arm64_boot_hat_unmap(PAGE_TEST_ALIAS2_VA) != 0 ||
	    arm64_boot_hat_unmap(PAGE_TEST_VA) != 0 ||
	    pp->p_share != 0 || pp->p_mapping != 0)
		return (-1);

	/*
	 * More mappings than a single hment page can hold, which drives
	 * hment_alloc() into growing the pool from the page_t freelist.
	 */
	free_before = arm64_page_freecnt();
	for (i = 0; i != PAGE_TEST_BULK; i++) {
		if (arm64_boot_hat_map(PAGE_TEST_BULK_VA + i * MMU_PAGESIZE,
		    pa, ARM64_HAT_WRITE) != 0)
			return (-1);
	}
	if (pp->p_share != PAGE_TEST_BULK ||
	    arm64_page_freecnt() >= free_before)
		return (-1);
	for (i = 0; i != PAGE_TEST_BULK; i++) {
		if (arm64_boot_hat_unmap(PAGE_TEST_BULK_VA +
		    i * MMU_PAGESIZE) != 0)
			return (-1);
	}
	if (pp->p_share != 0 || pp->p_mapping != 0)
		return (-1);

	/*
	 * Remapping a live VA without unmapping it first must retire the
	 * stale hment, or the displaced page keeps a phantom share count
	 * and can never be freed.
	 */
	old_pp = arm64_page_alloc();
	new_pp = arm64_page_alloc();
	if (old_pp == 0 || new_pp == 0)
		return (-1);
	old_pa = arm64_page_pa(old_pp);
	new_pa = arm64_page_pa(new_pp);
	*(volatile uint64_t *)(uintptr_t)new_pa = PAGE_TEST_PATTERN;
	if (arm64_boot_hat_map(PAGE_TEST_REMAP_VA, old_pa,
	    ARM64_HAT_WRITE) != 0 || old_pp->p_share != 1)
		return (-1);
	if (arm64_boot_hat_map(PAGE_TEST_REMAP_VA, new_pa,
	    ARM64_HAT_WRITE) != 0)
		return (-1);
	if (old_pp->p_share != 0 || old_pp->p_mapping != 0 ||
	    arm64_page_mapping_find(old_pa, PAGE_TEST_REMAP_VA, 0) != 0 ||
	    new_pp->p_share != 1 ||
	    arm64_boot_hat_lookup(PAGE_TEST_REMAP_VA, &translated) != 0 ||
	    translated != new_pa ||
	    *(volatile uint64_t *)(uintptr_t)PAGE_TEST_REMAP_VA !=
	    PAGE_TEST_PATTERN)
		return (-1);
	/* The displaced page is clean and goes straight back to the freelist. */
	if (arm64_page_free(old_pp) != 0 || !PP_ISFREE(old_pp))
		return (-1);
	if (arm64_boot_hat_unmap(PAGE_TEST_REMAP_VA) != 0 ||
	    new_pp->p_share != 0 || new_pp->p_mapping != 0 ||
	    arm64_page_free(new_pp) != 0)
		return (-1);

	/* User address-space mappings carry the ASID on the hment. */
	upp = arm64_page_alloc();
	if (upp == 0 || arm64_boot_hat_as_create(&as, PAGE_TEST_ASID) != 0)
		return (-1);
	upa = arm64_page_pa(upp);
	*(volatile uint64_t *)(uintptr_t)upa = PAGE_TEST_PATTERN;
	if (arm64_boot_hat_as_map(&as, PAGE_TEST_USER_VA, upa,
	    ARM64_HAT_WRITE) != 0)
		return (-1);
	hm = arm64_page_mapping_find(upa, PAGE_TEST_USER_VA, PAGE_TEST_ASID);
	if (hm == 0 || hm->hm_asid != PAGE_TEST_ASID || upp->p_share != 1)
		return (-1);
	old_ttbr0 = arm64_boot_hat_as_switch(&as);
	if (*(volatile uint64_t *)(uintptr_t)PAGE_TEST_USER_VA !=
	    PAGE_TEST_PATTERN) {
		arm64_boot_hat_as_restore(old_ttbr0);
		return (-1);
	}
	arm64_boot_hat_as_restore(old_ttbr0);
	if (arm64_boot_hat_as_unmap(&as, PAGE_TEST_USER_VA) != 0 ||
	    upp->p_share != 0 || upp->p_mapping != 0 ||
	    arm64_page_mapping_find(upa, PAGE_TEST_USER_VA,
	    PAGE_TEST_ASID) != 0)
		return (-1);

	/* Free returns pages to the freelist exactly once. */
	if (arm64_page_free(pp) != 0 || !PP_ISFREE(pp) ||
	    arm64_page_free(pp) == 0 || arm64_page_free(upp) != 0)
		return (-1);
	return (0);
}
