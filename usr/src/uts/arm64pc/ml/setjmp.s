/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 *
 * Kernel setjmp/longjmp primitives.  label_t contains the AAPCS64
 * callee-saved register set plus the saved stack and continuation PC.
 */

#include <sys/asm_linkage.h>

	.text
	.align	2

	ENTRY(setjmp)
	str	x30, [x0, #0]		/* continuation PC */
	mov	x1, sp
	str	x1, [x0, #8]
	stp	x19, x20, [x0, #16]
	stp	x21, x22, [x0, #32]
	stp	x23, x24, [x0, #48]
	stp	x25, x26, [x0, #64]
	stp	x27, x28, [x0, #80]
	stp	x29, x30, [x0, #96]
	mov	w0, wzr
	ret
	SET_SIZE(setjmp)

	ENTRY(longjmp)
	mov	x2, x0
	ldr	x1, [x2, #0]		/* branch target */
	ldr	x3, [x2, #8]
	mov	sp, x3
	ldp	x19, x20, [x2, #16]
	ldp	x21, x22, [x2, #32]
	ldp	x23, x24, [x2, #48]
	ldp	x25, x26, [x2, #64]
	ldp	x27, x28, [x2, #80]
	ldp	x29, x30, [x2, #96]
	mov	w0, #1
	br	x1
	SET_SIZE(longjmp)

	/*
	 * Save the current AAPCS64 non-volatile state in x0 and resume the
	 * context in x1.  Returning to a saved context makes the original call
	 * to arm64_context_switch() return normally.
	 */
	ENTRY(arm64_context_switch)
	str	x30, [x0, #0]
	mov	x2, sp
	str	x2, [x0, #8]
	stp	x19, x20, [x0, #16]
	stp	x21, x22, [x0, #32]
	stp	x23, x24, [x0, #48]
	stp	x25, x26, [x0, #64]
	stp	x27, x28, [x0, #80]
	stp	x29, x30, [x0, #96]

	mov	x4, x1
	ldr	x2, [x4, #0]
	ldr	x3, [x4, #8]
	mov	sp, x3
	ldp	x19, x20, [x4, #16]
	ldp	x21, x22, [x4, #32]
	ldp	x23, x24, [x4, #48]
	ldp	x25, x26, [x4, #64]
	ldp	x27, x28, [x4, #80]
	ldp	x29, x30, [x4, #96]
	br	x2
	SET_SIZE(arm64_context_switch)
