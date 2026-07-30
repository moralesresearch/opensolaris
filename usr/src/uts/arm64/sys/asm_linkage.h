/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 * Use is subject to license terms.
 */

#ifndef _SYS_ASM_LINKAGE_H
#define	_SYS_ASM_LINKAGE_H

#include <sys/stack.h>
#include <sys/trap.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _ASM

/*
 * AArch64 assembly linkage macros, parallel to intel/sys/asm_linkage.h
 */

/*
 * Assembler entry/end macros for function definitions.
 */
#define	ENTRY(name)			\
	.text;				\
	.align	4;			\
	.globl	name;			\
	.type	name, %function;	\
name:

#define	ENTRY_NP(name)			\
	.text;				\
	.align	4;			\
	.globl	name;			\
	.type	name, %function;	\
name:

#define	ENTRY2(name1, name2)		\
	.text;				\
	.align	4;			\
	.globl	name1;			\
	.globl	name2;			\
	.type	name1, %function;	\
	.type	name2, %function;	\
name1:					\
name2:

#define	ALTENTRY(name)			\
	.globl	name;			\
	.type	name, %function;	\
name:

#define	SET_SIZE(name)			\
	.size	name, . - name

/*
 * DGDEF/DGDEF2/DGDEF3 -- define global data of specified size.
 */
#define	DGDEF2(name, sz)		\
	.data;				\
	.align	8;			\
	.globl	name;			\
	.type	name, %object;		\
	.size	name, sz;		\
name:

#define	DGDEF3(name, sz, algn)		\
	.data;				\
	.align	algn;			\
	.globl	name;			\
	.type	name, %object;		\
	.size	name, sz;		\
name:

#define	DGDEF(name)			\
	DGDEF2(name, 8)

/*
 * Stack frame linkage macros.
 * AArch64 uses x29 (FP) and x30 (LR).
 */
#define	FRAME_PUSH()			\
	stp	x29, x30, [sp, #-16]!;	\
	mov	x29, sp

#define	FRAME_POP()			\
	ldp	x29, x30, [sp], #16

/*
 * Profiling support (placeholder -- disabled for now).
 */
#define	MCOUNT(name)

/*
 * AArch64 specific constants
 */
#define	CPTRSHIFT	3	/* log2(sizeof(void *)) for LP64 */
#define	CLONGSHIFT	3	/* log2(sizeof(long)) for LP64 */
#define	CPTRSIZE	8	/* sizeof(void *) for LP64 */
#define	CLONGSIZE	8	/* sizeof(long) for LP64 */

/*
 * Instruction barrier macros
 */
#define	ISB	isb
#define	DSB_SY	dsb	sy
#define	DSB_ISH	dsb	ish
#define	DMB_SY	dmb	sy
#define	DMB_ISH	dmb	ish

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ASM_LINKAGE_H */
