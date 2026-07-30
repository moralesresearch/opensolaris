/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/asm_linkage.h>

/*
 * Enter an AArch64 EL0 context.  The bootstrap escape SVC changes the saved
 * ELR/SPSR so exception return arrives at arm64_el0_resume below.
 */
	ENTRY(arm64_el0_enter)
	sub	sp, sp, #96
	stp	x19, x20, [sp, #0]
	stp	x21, x22, [sp, #16]
	stp	x23, x24, [sp, #32]
	stp	x25, x26, [sp, #48]
	stp	x27, x28, [sp, #64]
	stp	x29, x30, [sp, #80]
	msr	sp_el0, x1
	msr	elr_el1, x0
	mov	x2, #0x3c0		/* EL0t with D/A/I/F masked */
	msr	spsr_el1, x2
	eret
	SET_SIZE(arm64_el0_enter)

	ENTRY(arm64_el0_resume)
	ldp	x19, x20, [sp, #0]
	ldp	x21, x22, [sp, #16]
	ldp	x23, x24, [sp, #32]
	ldp	x25, x26, [sp, #48]
	ldp	x27, x28, [sp, #64]
	ldp	x29, x30, [sp, #80]
	add	sp, sp, #96
	ret
	SET_SIZE(arm64_el0_resume)
