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
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 *
 * Direct boot: early kernel setup for AArch64.
 * Parallel to i86pc/dboot/dboot_startkern.c.
 *
 * This code runs with the MMU OFF, using physical addresses.
 * It builds a QEMU-virt identity map, enables the MMU, and jumps to the
 * kernel entry point (_locore_start).
 */

#include <sys/types.h>

/*
 * AArch64 page table entry bits (4K granule, stage 1).
 */
#define	PTE_VALID	(1ULL << 0)
#define	PTE_TABLE	(1ULL << 1)	/* Table descriptor (L0-L2) */
#define	PTE_BLOCK	(0ULL << 1)	/* Block descriptor (L1-L2) */
#define	PTE_AF		(1ULL << 10)	/* Access Flag */
#define	PTE_SH_IS	(3ULL << 8)	/* Inner Shareable */
#define	PTE_AP_RW	(0ULL << 6)	/* R/W from EL1 */
#define	PTE_UXN		(1ULL << 54)	/* Unprivileged Execute Never */
#define	PTE_PXN		(1ULL << 53)	/* Privileged Execute Never */

/* MAIR attribute indices (must match MAIR_EL1 configuration) */
#define	PTE_ATTR_DEV	(0ULL << 2)	/* Device-nGnRnE (MAIR index 0) */
#define	PTE_ATTR_WB	(3ULL << 2)	/* Normal write-back (index 3) */

/* Page/block size constants */
#define	PAGE_SIZE	4096ULL
#define	L1_SHIFT	30		/* 1GB blocks */
#define	ENTRIES_PER_TABLE	512

/*
 * The bootstrap is identity mapped through TTBR0.  TTBR1 is deliberately
 * disabled until the OpenSolaris HAT owns the final kernel page tables.
 */
/*
 * SCTLR_EL1 RES1 bits for ARMv8-A: 29, 28, 23, 22, 20, 11.  This must match
 * SCTLR_EL1_RES1 in dboot_arm64.S; enable_mmu() read-modify-writes SCTLR_EL1
 * and must not propagate a value with any of them cleared.
 */
#define	SCTLR_EL1_RES1	((1ULL << 29) | (1ULL << 28) | (1ULL << 23) | \
			(1ULL << 22) | (1ULL << 20) | (1ULL << 11))

#define	VA_BITS			48
#define	TCR_T0SZ_VAL		(64 - VA_BITS)	/* = 16 */
#define	TCR_T1SZ_VAL		(64 - VA_BITS)	/* = 16 */

/*
 * Boot page tables — statically allocated.
 * Keep these outside the normal BSS: locore clears BSS after enabling the
 * MMU, and clearing an active translation table would fault immediately.
 */
static uint64_t ttbr0_l0[ENTRIES_PER_TABLE]
    __attribute__((aligned(PAGE_SIZE), section(".data.boot"))) = { 0 };
static uint64_t ttbr0_l1[ENTRIES_PER_TABLE]
    __attribute__((aligned(PAGE_SIZE), section(".data.boot"))) = { 0 };

/*
 * Early console output via UART (PL011 on QEMU virt).
 * QEMU virt machine PL011 UART is at physical 0x09000000.
 */
#define	UART_BASE	0x09000000ULL

static void
dboot_putc(char c)
{
	volatile uint32_t *uart = (volatile uint32_t *)UART_BASE;

	/* PL011: write data register (offset 0x000) */
	*uart = (uint32_t)c;
}

static void
dboot_puts(const char *s)
{
	while (*s) {
		if (*s == '\n')
			dboot_putc('\r');
		dboot_putc(*s++);
	}
}

/*
 * Build identity mapping (TTBR0): map physical 0-4GB as 1GB blocks.
 * This is temporary — removed after kernel is running.
 */
static void
build_identity_map(void)
{
	uint64_t i;

	/* L0 entry 0 -> L1 table */
	ttbr0_l0[0] = (uint64_t)ttbr0_l1 | PTE_VALID | PTE_TABLE;

	/* QEMU virt MMIO is below 1GB; RAM starts at 0x40000000. */
	for (i = 0; i < 4; i++) {
		uint64_t attrs;
		if (i == 0) {
			attrs = PTE_ATTR_DEV | PTE_AF | PTE_AP_RW |
			    PTE_UXN | PTE_PXN;
		} else {
			attrs = PTE_ATTR_WB | PTE_AF | PTE_SH_IS |
			    PTE_AP_RW | PTE_UXN;
		}
		ttbr0_l1[i] = (i << L1_SHIFT) | PTE_VALID | PTE_BLOCK | attrs;
	}
}

/*
 * Enable the MMU with the page tables we built.
 */
static void
enable_mmu(void)
{
	uint64_t tcr, mair, sctlr;

	/*
	 * Set MAIR_EL1 — memory attribute table.
	 * Index 0: Device-nGnRnE  (0x00)
	 * Index 1: Normal NC       (0x44)
	 * Index 2: Normal WT       (0xBB)
	 * Index 3: Normal WB       (0xFF)
	 */
	mair = (0x00ULL << 0) |	/* Index 0: Device */
	    (0x44ULL << 8) |		/* Index 1: NC */
	    (0xBBULL << 16) |		/* Index 2: WT */
	    (0xFFULL << 24);		/* Index 3: WB */
	__asm__ __volatile__("msr mair_el1, %0" :: "r"(mair));

	/*
	 * Set TCR_EL1 — translation control.
	 */
	tcr = ((uint64_t)TCR_T0SZ_VAL << 0) |	/* T0SZ */
	    ((uint64_t)TCR_T1SZ_VAL << 16) |	/* T1SZ */
	    (0x0ULL << 14) |	/* TG0 = 4KB */
	    (0x2ULL << 30) |	/* TG1 = 4KB */
	    (0x1ULL << 8) |	/* IRGN0 = WB WA */
	    (0x1ULL << 10) |	/* ORGN0 = WB WA */
	    (0x3ULL << 12) |	/* SH0 = Inner shareable */
	    (0x1ULL << 24) |	/* IRGN1 = WB WA */
	    (0x1ULL << 26) |	/* ORGN1 = WB WA */
	    (0x3ULL << 28) |	/* SH1 = Inner shareable */
	    (1ULL << 23) |	/* EPD1: TTBR1 disabled */
	    (0x2ULL << 32);	/* IPS = 40-bit PA */
	__asm__ __volatile__("msr tcr_el1, %0" :: "r"(tcr));
	__asm__ __volatile__("isb");

	/*
	 * Set TTBR0_EL1 and TTBR1_EL1.
	 */
	__asm__ __volatile__("msr ttbr0_el1, %0" :: "r"((uint64_t)ttbr0_l0));
	__asm__ __volatile__("msr ttbr1_el1, xzr");
	__asm__ __volatile__("isb");

	/*
	 * Invalidate TLBs.
	 */
	__asm__ __volatile__("tlbi vmalle1");
	__asm__ __volatile__("dsb nsh");
	__asm__ __volatile__("isb");

	/*
	 * Enable MMU + caches (SCTLR_EL1).
	 */
	__asm__ __volatile__("mrs %0, sctlr_el1" : "=r"(sctlr));
	sctlr |= SCTLR_EL1_RES1; /* Never clear RES1 across a read-modify-write */
	sctlr |= (1ULL << 0);	/* M: MMU enable */
	sctlr |= (1ULL << 2);	/* C: Data cache enable */
	sctlr |= (1ULL << 12);	/* I: Instruction cache enable */
	sctlr |= (1ULL << 3);	/* SA: Stack alignment check EL1 */
	sctlr |= (1ULL << 4);	/* SA0: Stack alignment check EL0 */
	sctlr &= ~(1ULL << 19); /* WXN: Don't enforce W^X for now */
	__asm__ __volatile__("msr sctlr_el1, %0" :: "r"(sctlr));
	__asm__ __volatile__("isb");
}

/* PSCI secondary CPUs enter with the MMU off and reuse the BSP identity map. */
void
dboot_secondary_mmu(void)
{
	enable_mmu();
}

/*
 * dboot_startkern — main C entry point for early boot.
 * Called from dboot_arm64.S with MMU off.
 *
	 * Sets up page tables, enables MMU, then jumps to _locore_start.
 */
void
dboot_startkern(uint64_t fdt_phys)
{
	extern void _locore_start(uint64_t, uint64_t);

	dboot_puts("\nOpenSolaris AArch64 dboot starting...\n");

	/* Build page tables */
	build_identity_map();

	dboot_puts("Page tables built. Enabling MMU...\n");

	/* Enable MMU */
	enable_mmu();

	dboot_puts("MMU enabled. Jumping to kernel...\n");

	/*
	 * Jump to the identity-mapped kernel entry point.
	 * x0 = boot_args (NULL for now, will be filled from FDT)
	 * x1 = FDT physical address
	 *
	 * A later milestone moves the full kernel into TTBR1 space after the
	 * VM/HAT layer owns the final translation tables.
	 */
	_locore_start(0, fdt_phys);

	/* Should never return */
	for (;;)
		;
}
