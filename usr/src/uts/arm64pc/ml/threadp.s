/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/asm_linkage.h>

	.text
	.align	2

	/* Non-inline implementation used by generic kernel objects. */
	ENTRY(threadp)
	mrs	x0, tpidr_el1
	ret
	SET_SIZE(threadp)
