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
 * Direct boot entry point for AArch64.
 * Parallel to i86pc/dboot/dboot_grub.s.
 *
 * This is the very first code that runs when the kernel image is loaded.
 * Entry conditions (from UEFI or U-Boot):
 *   - EL2 or EL1 (QEMU virt enters at EL1 with -kernel)
 *   - MMU off (flat/identity mapping)
 *   - x0 = FDT physical address (when loaded by U-Boot/QEMU -kernel)
 *     OR x0 = EFI_HANDLE, x1 = EFI_SYSTEM_TABLE (when loaded by UEFI)
 *   - Caches may or may not be on
 */

/*
 * HCR_EL2 bits.
 */
#define	HCR_EL2_SWIO		(1 << 1)	/* Set/way invalidate override */
#define	HCR_EL2_RW		(1 << 31)	/* EL1 is AArch64 */

/*
 * CPTR_EL2 (E2H == 0).  Bits 13:12 and 9:0 are RES1; TFP (10), TTA (20) and
 * TCPAC (31) stay clear so EL1/EL0 keep FP/NEON and trace access.
 */
#define	CPTR_EL2_RES1		0x33ff

/*
 * SCTLR_EL1.  The RES1 base for ARMv8-A is bits 29, 28, 23, 22, 20 and 11
 * (0x30d00800); dropping any of them makes the write architecturally
 * CONSTRAINED UNPREDICTABLE.
 */
#define	SCTLR_EL1_SA		(1 << 3)	/* EL1 stack alignment check */
#define	SCTLR_EL1_SA0		(1 << 4)	/* EL0 stack alignment check */
#define	SCTLR_EL1_CP15BEN	(1 << 5)	/* AArch32 CP15 barriers */
#define	SCTLR_EL1_NTWI		(1 << 16)	/* Do not trap EL0 WFI */
#define	SCTLR_EL1_NTWE		(1 << 18)	/* Do not trap EL0 WFE */
#define	SCTLR_EL1_RES1		((1 << 29) | (1 << 28) | (1 << 23) | \
				(1 << 22) | (1 << 20) | (1 << 11))
#define	SCTLR_EL1_INIT		(SCTLR_EL1_RES1 | SCTLR_EL1_NTWI | \
				SCTLR_EL1_NTWE | SCTLR_EL1_CP15BEN | \
				SCTLR_EL1_SA | SCTLR_EL1_SA0)

/*
 * GICv3 EL2 CPU interface control.  Without SRE/Enable an EL1 access to the
 * ICC_*_EL1 system registers traps to EL2.
 */
#define	ICC_SRE_EL2		S3_4_C12_C9_5
#define	ICC_SRE_EL2_SRE		(1 << 0)
#define	ICC_SRE_EL2_ENABLE	(1 << 3)

/*
 * CNTHCTL_EL2 (E2H == 0): let EL1 read the physical counter and program the
 * EL1 physical timer without trapping to EL2.
 */
#define	CNTHCTL_EL2_EL1PCTEN	(1 << 0)
#define	CNTHCTL_EL2_EL1PCEN	(1 << 1)

/* SPSR_EL2: DAIF masked, return to EL1h (M[3:0] = 0b0101). */
#define	SPSR_EL1H_DAIF		0x3c5

	.section .text.boot, "ax"

	/*
	 * Shared EL2 preamble for the EL2 -> EL1 drop.  Both the primary and
	 * the PSCI secondary entry paths must configure EL2 identically; the
	 * caller supplies ELR_EL2 and SP_EL1 and issues the eret.
	 * \tmp and \tmp2 are clobbered.
	 */
	.macro	EL2_TO_EL1_PREP tmp, tmp2
	mov	\tmp, #HCR_EL2_RW
	orr	\tmp, \tmp, #HCR_EL2_SWIO
	msr	hcr_el2, \tmp

	/* Preserve CPTR_EL2 RES1 while leaving FP/NEON and trace untrapped. */
	mov	\tmp, #CPTR_EL2_RES1
	msr	cptr_el2, \tmp

	/* Hand the GICv3 system-register CPU interface to EL1. */
	mrs	\tmp, ICC_SRE_EL2
	orr	\tmp, \tmp, #ICC_SRE_EL2_SRE
	orr	\tmp, \tmp, #ICC_SRE_EL2_ENABLE
	msr	ICC_SRE_EL2, \tmp
	isb

	/* Give EL1 the physical counter and physical timer. */
	mov	\tmp, #CNTHCTL_EL2_EL1PCTEN
	orr	\tmp, \tmp, #CNTHCTL_EL2_EL1PCEN
	msr	cnthctl_el2, \tmp

	/* Put SCTLR_EL1 in a known, RES1-correct state. */
	ldr	\tmp2, =SCTLR_EL1_INIT
	msr	sctlr_el1, \tmp2

	mov	\tmp, #SPSR_EL1H_DAIF
	msr	spsr_el2, \tmp
	.endm

	.globl	_start
	.type	_start, %function
_start:
	/* QEMU does not promise a usable incoming stack. */
	mov	x19, x0		/* Preserve the FDT pointer. */
	adrp	x10, dboot_stack_end
	add	x10, x10, :lo12:dboot_stack_end
	mov	sp, x10

	/*
	 * Determine current exception level.
	 */
	mrs	x9, CurrentEL
	and	x9, x9, #0xc
	cmp	x9, #0x8	/* EL2? */
	b.eq	.Lat_el2
	cmp	x9, #0x4	/* EL1? */
	b.eq	.Lat_el1

	/* EL3 — not expected, halt */
	b	.

.Lat_el2:
	/* Configure EL2 and drop to EL1. */
	EL2_TO_EL1_PREP x10, x11

	/* ELR_EL2 = address of EL1 entry below */
	adr	x10, .Lat_el1
	msr	elr_el2, x10

	/* Set SP_EL1 to current stack */
	mov	x10, sp
	msr	sp_el1, x10

	eret	/* "Return" to EL1 */

.Lat_el1:
	/*
	 * Now running at EL1. Save the FDT pointer.
	 * x0 = FDT physical address (from bootloader).
	 */
	/*
	 * Disable MMU, caches (clean start).
	 */
	mrs	x10, sctlr_el1
	bic	x10, x10, #1		/* Clear M (MMU) */
	bic	x10, x10, #(1 << 2)	/* Clear C (data cache) */
	bic	x10, x10, #(1 << 12)	/* Clear I (instruction cache) */
	msr	sctlr_el1, x10
	isb

	/*
	 * Invalidate TLBs and caches.
	 */
	tlbi	vmalle1
	dsb	nsh
	isb

	ic	iallu
	dsb	nsh
	isb

	/*
	 * Call dboot_startkern(fdt_phys_addr).
	 * This function sets up page tables, enables the MMU,
	 * and jumps to _locore_start. It does not return.
	 */
	mov	x0, x19		/* FDT addr */
	bl	dboot_startkern

	/* Should not return */
	b	.

	.size	_start, . - _start

	/*
	 * PSCI CPU_ON entry.  x0 is the context ID supplied by the BSP and is
	 * used as the logical CPU index into ap_stacks[].
	 */
	.globl	secondary_start
	.type	secondary_start, %function
secondary_start:
	msr	daifset, #0xf
	mov	x19, x0

	/*
	 * PSCI starts secondaries at the caller's highest implemented non-secure
	 * exception level.  When QEMU virtualization is enabled that is EL2, even
	 * though the boot CPU has already dropped to EL1.  Normalize every AP to
	 * EL1h before programming its EL1 MMU and interrupt interface.
	 */
	mrs	x9, CurrentEL
	and	x9, x9, #0xc
	cmp	x9, #0x8
	b.ne	.Lsecondary_el1
	EL2_TO_EL1_PREP x10, x11
	adr	x10, .Lsecondary_el1
	msr	elr_el2, x10
	adrp	x1, ap_stacks
	add	x1, x1, :lo12:ap_stacks
	ldr	x2, [x1, x19, lsl #3]
	msr	sp_el1, x2
	eret

.Lsecondary_el1:
	mov	x0, x19
	adrp	x1, ap_stacks
	add	x1, x1, :lo12:ap_stacks
	ldr	x2, [x1, x19, lsl #3]
	mov	sp, x2
	bl	dboot_secondary_mmu
	mov	x0, x19
	b	mpcore_entry
	.size	secondary_start, . - secondary_start


	/*
	 * Temporary boot stack (8KB).
	 */
	.section .bss.boot, "aw", %nobits
	.align	4
dboot_stack:
	.space	8192
dboot_stack_end:
