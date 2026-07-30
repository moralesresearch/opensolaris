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
 * AArch64 EL1 exception vector table.
 * This is loaded into VBAR_EL1 during kernel initialization.
 *
 * The vector table has 16 entries, each 128 bytes (32 instructions) apart:
 *   - 4 entries for current EL with SP_EL0  (shouldn't happen in kernel)
 *   - 4 entries for current EL with SP_ELx  (kernel exceptions)
 *   - 4 entries for lower EL, AArch64       (user-mode traps/syscalls)
 *   - 4 entries for lower EL, AArch32       (32-bit compat, not supported)
 *
 * Each entry: Synchronous, IRQ, FIQ, SError
 */

#include <sys/asm_linkage.h>

/*
 * Macro: save all GP registers onto the stack (trap frame).
 * We save x0-x30, SP_EL0, ELR_EL1, SPSR_EL1, ESR_EL1, FAR_EL1.
 * Total: 36 × 8 = 288 bytes.
 */
.macro	save_context
	sub	sp, sp, #288
	stp	x0, x1, [sp, #0]
	stp	x2, x3, [sp, #16]
	stp	x4, x5, [sp, #32]
	stp	x6, x7, [sp, #48]
	stp	x8, x9, [sp, #64]
	stp	x10, x11, [sp, #80]
	stp	x12, x13, [sp, #96]
	stp	x14, x15, [sp, #112]
	stp	x16, x17, [sp, #128]
	stp	x18, x19, [sp, #144]
	stp	x20, x21, [sp, #160]
	stp	x22, x23, [sp, #176]
	stp	x24, x25, [sp, #192]
	stp	x26, x27, [sp, #208]
	stp	x28, x29, [sp, #224]
	str	x30, [sp, #240]

	/* Save SP_EL0 (user stack pointer) */
	mrs	x0, sp_el0
	str	x0, [sp, #248]

	/* Save ELR_EL1 (return address) */
	mrs	x0, elr_el1
	str	x0, [sp, #256]

	/* Save SPSR_EL1 (saved PSTATE) */
	mrs	x0, spsr_el1
	str	x0, [sp, #264]

	/* Save ESR_EL1 (exception syndrome) */
	mrs	x0, esr_el1
	str	x0, [sp, #272]

	/* Save FAR_EL1 (fault address) */
	mrs	x0, far_el1
	str	x0, [sp, #280]
.endm

/*
 * Macro: restore all GP registers and return from exception.
 */
.macro	restore_context
	/* Restore ELR_EL1 */
	ldr	x0, [sp, #256]
	msr	elr_el1, x0

	/* Restore SPSR_EL1 */
	ldr	x0, [sp, #264]
	msr	spsr_el1, x0

	/* Restore SP_EL0 */
	ldr	x0, [sp, #248]
	msr	sp_el0, x0

	ldp	x0, x1, [sp, #0]
	ldp	x2, x3, [sp, #16]
	ldp	x4, x5, [sp, #32]
	ldp	x6, x7, [sp, #48]
	ldp	x8, x9, [sp, #64]
	ldp	x10, x11, [sp, #80]
	ldp	x12, x13, [sp, #96]
	ldp	x14, x15, [sp, #112]
	ldp	x16, x17, [sp, #128]
	ldp	x18, x19, [sp, #144]
	ldp	x20, x21, [sp, #160]
	ldp	x22, x23, [sp, #176]
	ldp	x24, x25, [sp, #192]
	ldp	x26, x27, [sp, #208]
	ldp	x28, x29, [sp, #224]
	ldr	x30, [sp, #240]
	add	sp, sp, #288
	eret
.endm

/*
 * Macro: vector entry point stub.
 * Each vector entry is 128 bytes (0x80).  We branch to the real handler.
 */
.macro	vector_entry label
	.align	7	/* 128-byte alignment */
\label:
	b	\label\()_handler
.endm

/*
 * The vector table proper.
 * Must be 2048-byte aligned (0x800).
 */
	.section .vectors, "ax"
	.align	11	/* 2048-byte alignment */
	.globl	arm64_exception_vectors
arm64_exception_vectors:

	/* Current EL with SP_EL0 (should not happen in kernel mode) */
	vector_entry	el1t_sync
	vector_entry	el1t_irq
	vector_entry	el1t_fiq
	vector_entry	el1t_serror

	/* Current EL with SP_ELx (kernel-mode exceptions) */
	vector_entry	el1h_sync
	vector_entry	el1h_irq
	vector_entry	el1h_fiq
	vector_entry	el1h_serror

	/* Lower EL, AArch64 (user-mode traps) */
	vector_entry	el0_sync_64
	vector_entry	el0_irq_64
	vector_entry	el0_fiq_64
	vector_entry	el0_serror_64

	/* Lower EL, AArch32 (not supported) */
	vector_entry	el0_sync_32
	vector_entry	el0_irq_32
	vector_entry	el0_fiq_32
	vector_entry	el0_serror_32


/*
 * Exception handlers.
 */

	.text

/* Current EL, SP_EL0 — should never happen */
el1t_sync_handler:
el1t_irq_handler:
el1t_fiq_handler:
el1t_serror_handler:
	save_context
	mov	x0, sp		/* x0 = pointer to trap frame */
	mov	x1, #0		/* trap type: EL1t */
	bl	arm64_trap_fatal
	b	.		/* should not return */

/* Current EL, SP_ELx — kernel synchronous exception */
el1h_sync_handler:
	save_context
	mov	x0, sp
	mov	x1, #4		/* T_SYNC_EL1H */
	bl	arm64_trap
	restore_context

/* Current EL, SP_ELx — kernel IRQ */
el1h_irq_handler:
	save_context
	mov	x0, sp
	bl	arm64_irq_handler
	restore_context

/* Current EL, SP_ELx — kernel FIQ */
el1h_fiq_handler:
	save_context
	mov	x0, sp
	bl	arm64_fiq_handler
	restore_context

/* Current EL, SP_ELx — kernel SError */
el1h_serror_handler:
	save_context
	mov	x0, sp
	mov	x1, #7		/* T_SERROR_EL1H */
	bl	arm64_trap_fatal
	b	.

/* Lower EL, AArch64 — user synchronous (syscall, page fault, etc.) */
el0_sync_64_handler:
	save_context
	mov	x0, sp
	mov	x1, #8		/* T_SYNC_EL0_64 */
	bl	arm64_trap
	restore_context

/* Lower EL, AArch64 — user IRQ */
el0_irq_64_handler:
	save_context
	mov	x0, sp
	bl	arm64_irq_handler
	restore_context

/* Lower EL, AArch64 — user FIQ */
el0_fiq_64_handler:
	save_context
	mov	x0, sp
	bl	arm64_fiq_handler
	restore_context

/* Lower EL, AArch64 — user SError */
el0_serror_64_handler:
	save_context
	mov	x0, sp
	mov	x1, #11	/* T_SERROR_EL0_64 */
	bl	arm64_trap_fatal
	b	.

/* Lower EL, AArch32 — not supported */
el0_sync_32_handler:
el0_irq_32_handler:
el0_fiq_32_handler:
el0_serror_32_handler:
	save_context
	mov	x0, sp
	mov	x1, #12	/* T_SYNC_EL0_32 */
	bl	arm64_trap_fatal
	b	.
