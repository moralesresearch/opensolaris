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
 * void efi_enter_kernel(uint64_t entry, uint64_t dtb,
 *     const struct load_range *ranges, uint64_t nranges);
 *
 * Final hand-off from the UEFI loader to the kernel, called after
 * ExitBootServices().  UEFI runs with the MMU and caches on at EL2 or EL1;
 * the kernel's dboot entry expects the direct-boot environment instead
 * (MMU off, x0 = DTB).  Every range the loader wrote, and this routine
 * itself, is cleaned and invalidated to the point of coherency before the
 * MMU is turned off so that the uncached accesses that follow observe the
 * data that was written through the cache.
 */

	.text
	.globl	efi_enter_kernel
	.p2align 6
efi_enter_kernel:
	msr	daifset, #0xf
	mov	x19, x0
	mov	x20, x1

	/* Minimum D-cache line size: 4 << CTR_EL0.DminLine bytes. */
	mrs	x9, ctr_el0
	ubfx	x9, x9, #16, #4
	mov	x10, #4
	lsl	x10, x10, x9
	sub	x11, x10, #1

1:	cbz	x3, 3f
	ldp	x4, x5, [x2], #16
	add	x5, x4, x5
	bic	x4, x4, x11
2:	dc	civac, x4
	add	x4, x4, x10
	cmp	x4, x5
	b.lo	2b
	sub	x3, x3, #1
	b	1b

	/* This code keeps executing across the MMU-off transition. */
3:	adr	x4, efi_enter_kernel
	adr	x5, .Lenter_end
	bic	x4, x4, x11
4:	dc	civac, x4
	add	x4, x4, x10
	cmp	x4, x5
	b.lo	4b
	dsb	sy

	mrs	x9, CurrentEL
	and	x9, x9, #0xc
	cmp	x9, #0x8
	b.eq	.Lel2

	mrs	x9, sctlr_el1
	bic	x9, x9, #0x1		/* M */
	bic	x9, x9, #0x4		/* C */
	bic	x9, x9, #0x1000		/* I */
	msr	sctlr_el1, x9
	isb
	tlbi	vmalle1
	b	.Lmmu_off

.Lel2:
	mrs	x9, sctlr_el2
	bic	x9, x9, #0x1
	bic	x9, x9, #0x4
	bic	x9, x9, #0x1000
	msr	sctlr_el2, x9
	isb
	tlbi	alle2

.Lmmu_off:
	dsb	sy
	ic	iallu
	dsb	sy
	isb

	mov	x0, x20
	mov	x1, xzr
	mov	x2, xzr
	mov	x3, xzr
	br	x19
.Lenter_end:
