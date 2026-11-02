/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2026 OpenSolaris x86_64 Contributors.
 *
 * Minimal 4-level (PML4/PDPT/PD/PT) page-table engine for the amd64pc
 * bootstrap: 4 KiB map, lookup and unmap on the live address space,
 * with every intermediate table drawn from the page_t freelist.  The
 * 2 MiB identity map that dboot built for the low 4 GiB is left alone;
 * new mappings go into the kernel's high half (KERNELBASE-style, PML4
 * slot 511) and the user half.  The PTE bits are those of
 * uts/i86pc/vm/hat_pte.h; the full HAT (hat_i86.c, htable.c) replaces
 * this engine when the common VM is brought up.
 */

#include <sys/types.h>
#include <vm/page.h>
#include <sys/amd64pc.h>

#define	PT_VALID	0x001ULL
#define	PT_WRITABLE	0x002ULL
#define	PT_USER		0x004ULL
#define	PT_PAGESIZE	0x080ULL
#define	PT_NX		(1ULL << 63)
#define	PT_PADDR	0x000ffffffffff000ULL

#define	LEVEL_SHIFT(l)	(12 + 9 * (l))
#define	LEVEL_INDEX(va, l)	(((va) >> LEVEL_SHIFT(l)) & 511)

/* The high-half alias window used by the self test (-2 GiB). */
#define	HAT_TEST_VA	0xffffffff80000000ULL

#define	MSR_EFER	0xc0000080
#define	EFER_NXE	0x800

static uint64_t *pml4;
static uint64_t pt_nx;		/* PT_NX when EFER.NXE could be enabled */

int
hat_boot_init(void)
{
	uint32_t a, b, c, d;

	pml4 = (uint64_t *)(uintptr_t)(pc_getcr3() & PT_PADDR);
	pc_cpuid(0x80000001, 0, &a, &b, &c, &d);
	if ((d & (1U << 20)) != 0) {
		pc_wrmsr(MSR_EFER, pc_rdmsr(MSR_EFER) | EFER_NXE);
		pt_nx = PT_NX;
	}
	return (pml4 == 0 ? -1 : 0);
}

static int
canonical(uint64_t va)
{
	uint64_t top = va >> 47;

	return (top == 0 || top == 0x1ffff);
}

/*
 * Walk to the level-0 PTE for va, allocating zeroed page-table pages from
 * the page_t freelist when `create' is set.  Intermediate entries for
 * user addresses carry PT_USER so that the leaf decides access.
 */
static uint64_t *
hat_walk(uint64_t va, int create, uint64_t inter_flags)
{
	uint64_t *table = pml4;
	int level;

	for (level = 3; level > 0; level--) {
		uint64_t *e = &table[LEVEL_INDEX(va, level)];

		if ((*e & PT_VALID) == 0) {
			page_t *pp;

			if (!create)
				return (0);
			if ((pp = page_boot_alloc()) == 0)
				return (0);
			*e = page_boot_pa(pp) | PT_VALID | PT_WRITABLE |
			    inter_flags;
		} else if ((*e & PT_PAGESIZE) != 0) {
			/* Inside dboot's large-page identity map. */
			return (0);
		} else {
			*e |= inter_flags;
		}
		table = (uint64_t *)(uintptr_t)(*e & PT_PADDR);
	}
	return (&table[LEVEL_INDEX(va, 0)]);
}

int
hat_boot_map(uint64_t va, uint64_t pa, uint64_t flags)
{
	uint64_t *pte;

	if ((va & 0xfff) != 0 || (pa & 0xfff) != 0 || !canonical(va))
		return (-1);
	pte = hat_walk(va, 1, flags & PT_USER);
	if (pte == 0)
		return (-1);
	if ((*pte & PT_VALID) != 0) {
		/* Remap: tear the old translation down first. */
		*pte = 0;
		pc_invlpg(va);
	}
	*pte = pa | PT_VALID | (flags & (PT_WRITABLE | PT_USER | pt_nx));
	return (0);
}

int
hat_boot_unmap(uint64_t va)
{
	uint64_t *pte = hat_walk(va, 0, 0);

	if (pte == 0 || (*pte & PT_VALID) == 0)
		return (-1);
	*pte = 0;
	pc_invlpg(va);
	return (0);
}

int
hat_boot_lookup(uint64_t va, uint64_t *pa)
{
	uint64_t *table = pml4;
	int level;

	for (level = 3; level >= 0; level--) {
		uint64_t e = table[LEVEL_INDEX(va, level)];

		if ((e & PT_VALID) == 0)
			return (-1);
		if (level == 0 || (e & PT_PAGESIZE) != 0) {
			uint64_t mask = (1ULL << LEVEL_SHIFT(level)) - 1;

			*pa = (e & PT_PADDR & ~mask) | (va & mask);
			return (0);
		}
		table = (uint64_t *)(uintptr_t)(e & PT_PADDR);
	}
	return (-1);
}

extern int amd64pc_probe_read(uint64_t, uint64_t *);

/*
 * Alias a kernel data page into the high half, read and write through
 * both views, check lookup on both the identity and alias mappings, then
 * unmap and prove that the alias now faults.
 */
int
hat_selftest(void)
{
	static volatile uint64_t target[512] __attribute__((aligned(4096)));
	uint64_t pa = (uint64_t)(uintptr_t)target;
	uint64_t va = HAT_TEST_VA + 0x5000;
	uint64_t got = 0, tables = page_boot_freecnt();
	volatile uint64_t *alias = (volatile uint64_t *)(uintptr_t)va;

	target[7] = 0x0123456789abcdefULL;
	if (hat_boot_map(va, pa, PT_WRITABLE | PT_NX) != 0)
		return (-1);
	if (alias[7] != 0x0123456789abcdefULL)
		return (-2);
	alias[8] = 0xfedcba9876543210ULL;
	if (target[8] != 0xfedcba9876543210ULL)
		return (-3);
	if (hat_boot_lookup(va + 0x38, &got) != 0 || got != pa + 0x38)
		return (-4);
	if (hat_boot_lookup(pa, &got) != 0 || got != pa)
		return (-5);
	if (page_boot_freecnt() >= tables)
		return (-6);	/* page tables came from the freelist */
	if (hat_boot_unmap(va) != 0)
		return (-7);
	if (amd64pc_probe_read(va, &got) != -1)
		return (-8);
	if (hat_boot_lookup(va, &got) != -1)
		return (-9);
	return (0);
}
