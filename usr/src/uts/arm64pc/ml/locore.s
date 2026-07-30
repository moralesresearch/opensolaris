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
 * AArch64 kernel entry point (locore.s).
 * Parallel to i86pc/ml/locore.s.
 *
 * Entry conditions (from dboot or UEFI stub):
 *   - Running at EL1
 *   - MMU enabled with identity mapping + kernel VA mapping
 *   - x0 = pointer to boot_args structure
 *   - x1 = device tree blob (FDT) physical address
 *   - SP = valid kernel stack
 */

#include <sys/asm_linkage.h>

	.text

	/*
	 * _locore_start: kernel entry point after dboot.
	 * Called with MMU on, identity mapping active.
	 */
	.globl	_locore_start
	.type	_locore_start, %function
_locore_start:
	/*
	 * Mask all interrupts during early init.
	 */
	msr	daifset, #0xf

	/*
	 * Save boot arguments.
	 */
	adrp	x2, boot_args_ptr
	str	x0, [x2, :lo12:boot_args_ptr]
	adrp	x2, fdt_phys_addr
	str	x1, [x2, :lo12:fdt_phys_addr]

	/*
	 * Set up the exception vector table.
	 */
	adrp	x0, arm64_exception_vectors
	add	x0, x0, :lo12:arm64_exception_vectors
	msr	vbar_el1, x0
	isb

	/*
	 * Set up a proper kernel stack.
	 * t0stack is the initial kernel stack (defined in startup.c).
	 */
	adrp	x0, t0stack
	add	x0, x0, :lo12:t0stack
	add	x0, x0, #4, lsl #12	/* Top of 16KB stack */
	sub	x0, x0, #16		/* Keep it 16-byte aligned */
	mov	sp, x0

	/*
	 * Enable FP/NEON access (CPACR_EL1.FPEN = 0b11).
	 * Some C code may use FP for struct copies.
	 */
	mov	x0, #(3 << 20)
	msr	cpacr_el1, x0
	isb

	/*
	 * Clear BSS section.
	 */
	adrp	x0, __bss_start
	add	x0, x0, :lo12:__bss_start
	adrp	x1, __bss_end
	add	x1, x1, :lo12:__bss_end
1:
	cmp	x0, x1
	b.ge	2f
	str	xzr, [x0], #8
	b	1b
2:

	/*
	 * Call mlsetup() — machine-level setup.
	 * This initializes CPU data structures, GIC, timer, etc.
	 */
	bl	mlsetup

	/* The bootstrap image stops before the common kernel is linkable. */
#ifdef ARM64_BOOTSTRAP
	bl	arm64_bootstrap_main
#else
	bl	main
#endif

	/* Should never reach here */
	b	.

	SET_SIZE(_locore_start)


	/*
	 * Secondary CPU entry point (called via PSCI CPU_ON).
	 * x0 = cpu_id (MPIDR affinity value)
	 */
	.globl	mpcore_entry
	.type	mpcore_entry, %function
mpcore_entry:
	/* Mask interrupts */
	msr	daifset, #0xf

	/*
	 * Set up exception vectors.
	 */
	adrp	x1, arm64_exception_vectors
	add	x1, x1, :lo12:arm64_exception_vectors
	msr	vbar_el1, x1
	isb

	/*
	 * Enable FP/NEON.
	 */
	mov	x1, #(3 << 20)
	msr	cpacr_el1, x1
	isb

	/*
	 * Look up our per-CPU stack.
	 * mp_startup() will have set up cpu[cpuid]->cpu_stack.
	 * For now, x0 has our MPIDR-derived index.
	 * The startup code stores the stack pointer for us.
	 */
	adrp	x1, ap_stacks
	add	x1, x1, :lo12:ap_stacks
	ldr	x1, [x1, x0, lsl #3]	/* ap_stacks[cpuid] */
	mov	sp, x1

	/*
	 * Call mp_startup(cpuid).
	 * x0 still holds the cpu ID.
	 */
	bl	mp_startup

	/* Should not return */
	b	.

	SET_SIZE(mpcore_entry)


	/*
	 * Data section: global pointers for boot info.
	 */
	.data
	.align	3

	.globl	boot_args_ptr
boot_args_ptr:
	.quad	0

	.globl	fdt_phys_addr
fdt_phys_addr:
	.quad	0

	.globl	ap_stacks
ap_stacks:
	.space	(256 * 8)	/* Room for NCPU stack pointers */
