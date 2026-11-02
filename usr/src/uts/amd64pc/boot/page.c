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
 * Boot-time physical memory for the amd64pc bootstrap, described by the
 * common page_t (uts/common/vm/page.h) exactly as the AArch64 port does.
 * The largest usable RAM range below 4 GiB that lies above the kernel
 * image backs a page_t array and a freelist; page-table pages and test
 * pages come from here.  The Multiboot2 information structure, the
 * kernel and the page_t array itself are never placed on the freelist.
 *
 * Allocation is boot-CPU only and unlocked during the bootstrap; the
 * common page layer's p_selock discipline arrives with the full VM.
 */

#include <sys/types.h>
#include <vm/page.h>
#include <sys/amd64pc.h>

#define	BOOT_PAGESIZE	4096ULL
#define	BOOT_PAGESHIFT	12
#define	MAX_MANAGED	(256ULL << 20)	/* bytes of RAM given page_ts */

extern char __image_end[];

static page_t *page_array;
static pfn_t page_base;
static uint64_t page_npages;
static uint64_t page_nfree;
static page_t *page_freelist;

static uint64_t
roundup_pg(uint64_t v)
{
	return ((v + BOOT_PAGESIZE - 1) & ~(BOOT_PAGESIZE - 1));
}

uint64_t
page_boot_pa(const struct page *pp)
{
	return ((uint64_t)pp->p_pagenum << BOOT_PAGESHIFT);
}

uint64_t
page_boot_total(void)
{
	return (page_npages);
}

uint64_t
page_boot_freecnt(void)
{
	return (page_nfree);
}

pfn_t
page_pptonum(page_t *pp)
{
	return (pp->p_pagenum);
}

page_t *
page_numtopp_nolock(pfn_t pfn)
{
	if (page_array == 0 || pfn < page_base ||
	    pfn >= page_base + page_npages)
		return (0);
	return (&page_array[pfn - page_base]);
}

static void
freelist_push(page_t *pp)
{
	pp->p_next = page_freelist;
	page_freelist = pp;
	PP_SETFREE(pp);
	page_nfree++;
}

int
page_boot_init(const struct amd64pc_bootinfo *bi)
{
	uint64_t kend = roundup_pg((uint64_t)(uintptr_t)__image_end);
	uint64_t best_base = 0, best_size = 0;
	uint64_t mbi_lo = bi->bi_mbi & ~(BOOT_PAGESIZE - 1);
	uint64_t mbi_hi = roundup_pg(bi->bi_mbi + bi->bi_mbi_size);
	uint64_t array_bytes, first, pa, i;
	uint32_t m;

	for (m = 0; m < bi->bi_nmem; m++) {
		uint64_t lo = roundup_pg(bi->bi_mem[m].ms_base);
		uint64_t hi = (bi->bi_mem[m].ms_base +
		    bi->bi_mem[m].ms_size) & ~(BOOT_PAGESIZE - 1);

		if (lo < kend)
			lo = kend;
		if (hi > (4ULL << 30))
			hi = 4ULL << 30;
		if (hi > lo && hi - lo > best_size) {
			best_base = lo;
			best_size = hi - lo;
		}
	}
	if (best_size < (4ULL << 20))
		return (-1);
	if (best_size > MAX_MANAGED)
		best_size = MAX_MANAGED;

	/* The page_t array occupies the start of the managed range. */
	page_npages = best_size >> BOOT_PAGESHIFT;
	array_bytes = roundup_pg(page_npages * sizeof (page_t));
	page_array = (page_t *)(uintptr_t)best_base;
	page_base = (pfn_t)(best_base >> BOOT_PAGESHIFT);
	{
		uint8_t *b = (uint8_t *)page_array;

		for (i = 0; i < array_bytes; i++)
			b[i] = 0;
	}
	for (i = 0; i < page_npages; i++)
		page_array[i].p_pagenum = page_base + i;

	first = best_base + array_bytes;
	/* Push in descending order so allocation hands out low pages first. */
	for (pa = best_base + best_size - BOOT_PAGESIZE; pa >= first;
	    pa -= BOOT_PAGESIZE) {
		if (pa + BOOT_PAGESIZE > mbi_lo && pa < mbi_hi)
			continue;
		freelist_push(&page_array[(pa >> BOOT_PAGESHIFT) - page_base]);
	}
	return (0);
}

page_t *
page_boot_alloc(void)
{
	page_t *pp = page_freelist;
	uint64_t *w;
	int i;

	if (pp == 0)
		return (0);
	page_freelist = pp->p_next;
	pp->p_next = 0;
	PP_CLRFREE(pp);
	page_nfree--;
	w = (uint64_t *)(uintptr_t)page_boot_pa(pp);
	for (i = 0; i < (int)(BOOT_PAGESIZE / sizeof (*w)); i++)
		w[i] = 0;
	return (pp);
}

void
page_boot_free(page_t *pp)
{
	if (pp == 0 || PP_ISFREE(pp))
		amd64pc_panic("page_boot_free: bad or already free page");
	freelist_push(pp);
}

int
page_selftest(void)
{
	page_t *pp[8];
	uint64_t before = page_nfree;
	int i, j;

	for (i = 0; i < 8; i++) {
		pp[i] = page_boot_alloc();
		if (pp[i] == 0 || PP_ISFREE(pp[i]))
			return (-1);
		if (page_numtopp_nolock(page_pptonum(pp[i])) != pp[i])
			return (-2);
		*(volatile uint64_t *)(uintptr_t)page_boot_pa(pp[i]) =
		    0x1000 + i;
		for (j = 0; j < i; j++)
			if (pp[j] == pp[i])
				return (-3);
	}
	for (i = 0; i < 8; i++)
		if (*(volatile uint64_t *)(uintptr_t)page_boot_pa(pp[i]) !=
		    (uint64_t)(0x1000 + i))
			return (-4);
	if (page_nfree != before - 8)
		return (-5);
	for (i = 7; i >= 0; i--)
		page_boot_free(pp[i]);
	if (page_nfree != before || !PP_ISFREE(pp[0]))
		return (-6);
	return (0);
}
