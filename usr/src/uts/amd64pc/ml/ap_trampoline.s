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
 * Application-processor start-up trampoline; the counterpart of
 * i86pc/ml/mpcore.s.  boot/smp.c copies this page to
 * AMD64PC_AP_TRAMPOLINE (a real-mode page below 1 MiB), fills in the data
 * slots at the end and sends INIT-SIPI-SIPI.  The AP starts in real mode
 * at CS = AMD64PC_AP_TRAMPOLINE >> 4, IP = 0, and walks through protected
 * mode into long mode on the BSP's page tables before jumping to the C
 * entry point with %edi = logical CPU id on its own stack.
 */

#define	_ASM
#include <sys/controlregs.h>

#define	TRAMP		0x8000
#define	TOFF(sym)	((sym) - ap_tramp_start)
#define	TADDR(sym)	(TRAMP + TOFF(sym))

	.text
	.p2align 12
	.globl	ap_tramp_start, ap_tramp_end
	.globl	ap_tramp_cr3, ap_tramp_efer, ap_tramp_stack
	.globl	ap_tramp_entry, ap_tramp_cpu

	.code16
ap_tramp_start:
	cli
	cld
	movw	%cs, %ax
	movw	%ax, %ds
	lgdtl	TOFF(ap_tramp_gdtr)
	movl	%cr0, %eax
	orl	$CR0_PE, %eax
	movl	%eax, %cr0
	ljmpl	$0x08, $TADDR(ap_tramp_pm32)

	.code32
ap_tramp_pm32:
	movw	$0x10, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss

	movl	%cr4, %eax
	orl	$(CR4_PAE | CR4_PGE), %eax
	movl	%eax, %cr4

	movl	TADDR(ap_tramp_cr3), %eax
	movl	%eax, %cr3

	/* Same EFER as the BSP (LME, NXE, SCE); LMA is read-only. */
	movl	$MSR_AMD_EFER, %ecx
	movl	TADDR(ap_tramp_efer), %eax
	andl	$~AMD_EFER_LMA, %eax
	xorl	%edx, %edx
	wrmsr

	movl	%cr0, %eax
	orl	$(CR0_PG | CR0_WP), %eax
	movl	%eax, %cr0
	ljmp	$0x18, $TADDR(ap_tramp_lm64)

	.code64
ap_tramp_lm64:
	xorl	%eax, %eax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	movw	%ax, %fs
	movw	%ax, %gs
	movq	TADDR(ap_tramp_stack), %rsp
	movl	TADDR(ap_tramp_cpu), %edi
	movq	TADDR(ap_tramp_entry), %rax
	xorl	%ebp, %ebp
	jmp	*%rax

	.p2align 3
ap_tramp_gdt:
	.quad	0
	.quad	0x00cf9a000000ffff	/* 0x08: 32-bit code */
	.quad	0x00cf92000000ffff	/* 0x10: 32-bit data */
	.quad	0x00af9a000000ffff	/* 0x18: 64-bit code */
ap_tramp_gdt_end:

	.p2align 3
ap_tramp_gdtr:
	.short	ap_tramp_gdt_end - ap_tramp_gdt - 1
	.long	TADDR(ap_tramp_gdt)

	.p2align 3
ap_tramp_cr3:
	.quad	0
ap_tramp_efer:
	.quad	0
ap_tramp_stack:
	.quad	0
ap_tramp_entry:
	.quad	0
ap_tramp_cpu:
	.long	0
ap_tramp_end:
