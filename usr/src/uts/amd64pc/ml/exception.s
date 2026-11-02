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
 * Copyright 2026 OpenSolaris x86_64 Contributors.
 *
 * Trap, interrupt and system-call entry for the amd64pc bootstrap.
 *
 * Every vector enters through a 16-byte stub at amd64pc_trap_stubs +
 * 16 * vector.  The stub normalises the hardware frame (pushing a zero
 * error code where the CPU does not) and the common path then builds a
 * complete `struct regs' from uts/intel/amd64/sys/privregs.h, so C code
 * sees exactly the trap frame layout the OpenSolaris amd64 kernel uses.
 * Field offsets are checked against the C structure at compile time in
 * os/trap.c.
 */

#define	REGOFF_SAVFP	0
#define	REGOFF_SAVPC	8
#define	REGOFF_RDI	16
#define	REGOFF_RSI	24
#define	REGOFF_RDX	32
#define	REGOFF_RCX	40
#define	REGOFF_R8	48
#define	REGOFF_R9	56
#define	REGOFF_RAX	64
#define	REGOFF_RBX	72
#define	REGOFF_RBP	80
#define	REGOFF_R10	88
#define	REGOFF_R11	96
#define	REGOFF_R12	104
#define	REGOFF_R13	112
#define	REGOFF_R14	120
#define	REGOFF_R15	128
#define	REGOFF_FSBASE	136
#define	REGOFF_GSBASE	144
#define	REGOFF_DS	152
#define	REGOFF_ES	160
#define	REGOFF_FS	168
#define	REGOFF_GS	176
#define	REGOFF_TRAPNO	184
#define	REGOFF_ERR	192
#define	REGOFF_RIP	200

	.text

	/*
	 * Vectors 8, 10-14, 17, 21, 29 and 30 push an error code; every
	 * other stub supplies a zero so the frame shape is uniform.
	 */
	.p2align 4
	.globl	amd64pc_trap_stubs
amd64pc_trap_stubs:
	.set	vec, 0
	.rept	256
	.p2align 4
	.if	(vec == 8) || ((vec >= 10) && (vec <= 14)) || (vec == 17) || \
		(vec == 21) || (vec == 29) || (vec == 30)
	pushq	$vec
	.else
	pushq	$0
	pushq	$vec
	.endif
	jmp	amd64pc_trap_common
	.set	vec, vec + 1
	.endr

	.p2align 4
amd64pc_trap_common:
	subq	$REGOFF_TRAPNO, %rsp
	movq	%rdi, REGOFF_RDI(%rsp)
	movq	%rsi, REGOFF_RSI(%rsp)
	movq	%rdx, REGOFF_RDX(%rsp)
	movq	%rcx, REGOFF_RCX(%rsp)
	movq	%r8, REGOFF_R8(%rsp)
	movq	%r9, REGOFF_R9(%rsp)
	movq	%rax, REGOFF_RAX(%rsp)
	movq	%rbx, REGOFF_RBX(%rsp)
	movq	%rbp, REGOFF_RBP(%rsp)
	movq	%r10, REGOFF_R10(%rsp)
	movq	%r11, REGOFF_R11(%rsp)
	movq	%r12, REGOFF_R12(%rsp)
	movq	%r13, REGOFF_R13(%rsp)
	movq	%r14, REGOFF_R14(%rsp)
	movq	%r15, REGOFF_R15(%rsp)
	movq	$0, REGOFF_FSBASE(%rsp)
	movq	$0, REGOFF_GSBASE(%rsp)
	xorl	%eax, %eax
	movw	%ds, %ax
	movq	%rax, REGOFF_DS(%rsp)
	movw	%es, %ax
	movq	%rax, REGOFF_ES(%rsp)
	movw	%fs, %ax
	movq	%rax, REGOFF_FS(%rsp)
	movw	%gs, %ax
	movq	%rax, REGOFF_GS(%rsp)

	/* mdb-style frame link so stack walkers can cross the trap. */
	movq	REGOFF_RIP(%rsp), %rax
	movq	%rax, REGOFF_SAVPC(%rsp)
	movq	%rbp, REGOFF_SAVFP(%rsp)
	leaq	REGOFF_SAVFP(%rsp), %rbp

	cld
	movq	%rsp, %rdi
	call	amd64pc_trap

	movq	REGOFF_RDI(%rsp), %rdi
	movq	REGOFF_RSI(%rsp), %rsi
	movq	REGOFF_RDX(%rsp), %rdx
	movq	REGOFF_RCX(%rsp), %rcx
	movq	REGOFF_R8(%rsp), %r8
	movq	REGOFF_R9(%rsp), %r9
	movq	REGOFF_RAX(%rsp), %rax
	movq	REGOFF_RBX(%rsp), %rbx
	movq	REGOFF_RBP(%rsp), %rbp
	movq	REGOFF_R10(%rsp), %r10
	movq	REGOFF_R11(%rsp), %r11
	movq	REGOFF_R12(%rsp), %r12
	movq	REGOFF_R13(%rsp), %r13
	movq	REGOFF_R14(%rsp), %r14
	movq	REGOFF_R15(%rsp), %r15
	addq	$REGOFF_RIP, %rsp
	iretq

	/*
	 * 64-bit SYSCALL entry (MSR_AMD_LSTAR).  %rcx holds the user %rip,
	 * %r11 the user %rflags; SFMASK has cleared IF.  The bootstrap runs
	 * one user context at a time, so a single saved user %rsp and a
	 * dedicated kernel stack suffice.  The C handler receives
	 * (%rax = number, %rdi, %rsi) and its return value goes back in %rax.
	 */
	.p2align 4
	.globl	amd64pc_syscall_entry
amd64pc_syscall_entry:
	movq	%rsp, amd64pc_syscall_ursp(%rip)
	movq	amd64pc_syscall_kstack(%rip), %rsp
	pushq	%rcx
	pushq	%r11
	movq	%rsi, %rdx
	movq	%rdi, %rsi
	movq	%rax, %rdi
	call	amd64pc_syscall
	popq	%r11
	popq	%rcx
	movq	amd64pc_syscall_ursp(%rip), %rsp
	sysretq

	/*
	 * void amd64pc_user_enter(uint64_t rip, uint64_t rsp, uint64_t cs,
	 *     uint64_t ss);
	 * Drop to ring 3 through an interrupt-return frame.
	 */
	.globl	amd64pc_user_enter
amd64pc_user_enter:
	pushq	%rcx		/* %ss */
	pushq	%rsi		/* %rsp */
	pushq	$0x202		/* %rflags: IF | reserved bit 1 */
	pushq	%rdx		/* %cs */
	pushq	%rdi		/* %rip */
	xorl	%eax, %eax
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx
	xorl	%edx, %edx
	xorl	%esi, %esi
	xorl	%edi, %edi
	xorl	%ebp, %ebp
	iretq

	/* Context primitives used by the user-mode harness and tests. */
	.globl	amd64pc_setjmp
amd64pc_setjmp:
	movq	%rbx, 0(%rdi)
	movq	%rbp, 8(%rdi)
	movq	%r12, 16(%rdi)
	movq	%r13, 24(%rdi)
	movq	%r14, 32(%rdi)
	movq	%r15, 40(%rdi)
	leaq	8(%rsp), %rax
	movq	%rax, 48(%rdi)
	movq	(%rsp), %rax
	movq	%rax, 56(%rdi)
	xorl	%eax, %eax
	ret

	.globl	amd64pc_longjmp
amd64pc_longjmp:
	movq	0(%rdi), %rbx
	movq	8(%rdi), %rbp
	movq	16(%rdi), %r12
	movq	24(%rdi), %r13
	movq	32(%rdi), %r14
	movq	40(%rdi), %r15
	movq	48(%rdi), %rsp
	movl	%esi, %eax
	testl	%eax, %eax
	jnz	1f
	incl	%eax
1:	jmp	*56(%rdi)

	/*
	 * int amd64pc_probe_read(uint64_t va, uint64_t *out);
	 * Returns 0 and the quadword at va, or -1 if the load faulted; the
	 * page-fault handler redirects a fault at amd64pc_probe_insn to
	 * amd64pc_probe_fault (the bootstrap's t_lofault).
	 */
	.globl	amd64pc_probe_read, amd64pc_probe_insn, amd64pc_probe_fault
amd64pc_probe_read:
amd64pc_probe_insn:
	movq	(%rdi), %rax
	movq	%rax, (%rsi)
	xorl	%eax, %eax
	ret
amd64pc_probe_fault:
	movl	$-1, %eax
	ret

	/*
	 * Position-independent user programs, copied into user pages by
	 * os/user.c.  user_prog makes a round-trip system call, then exits
	 * with the value returned; user_fault touches an unmapped user page.
	 */
	.globl	amd64pc_user_prog, amd64pc_user_prog_end
amd64pc_user_prog:
	movl	$1, %eax		/* SYS_test */
	movabsq	$0x0505a1a0d15c0, %rdi
	movq	%rsp, %rsi
	syscall
	movq	%rax, %rdi
	movl	$2, %eax		/* SYS_exit */
	syscall
	ud2
amd64pc_user_prog_end:

	.globl	amd64pc_user_fault, amd64pc_user_fault_end
amd64pc_user_fault:
	movabsq	$0x0000008000400000, %rax
	movq	(%rax), %rax
	ud2
amd64pc_user_fault_end:

	.bss
	.p2align 3
	.globl	amd64pc_syscall_kstack
amd64pc_syscall_ursp:
	.quad	0
amd64pc_syscall_kstack:
	.quad	0
