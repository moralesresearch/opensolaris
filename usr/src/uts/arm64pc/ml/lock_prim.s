/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/asm_linkage.h>

	ENTRY(lock_try)
1:	ldaxrb	w1, [x0]
	cbnz	w1, 2f
	mov	w1, #0xff
	stxrb	w2, w1, [x0]
	cbnz	w2, 1b
	mov	w0, #1
	ret
2:	mov	w0, #0
	ret
	SET_SIZE(lock_try)

	ENTRY(lock_set)
1:	ldaxrb	w1, [x0]
	cbnz	w1, 3f
	mov	w1, #0xff
	stxrb	w2, w1, [x0]
	cbz	w2, 2f
	b	1b
2:	ret
3:	wfe
	b	1b
	SET_SIZE(lock_set)

	ENTRY(lock_clear)
	stlrb	wzr, [x0]
	sev
	ret
	SET_SIZE(lock_clear)

	.globl	lock_spin_try
	.set	lock_spin_try, lock_try
	.globl	ulock_try
	.set	ulock_try, lock_try
	.globl	ulock_clear
	.set	ulock_clear, lock_clear
