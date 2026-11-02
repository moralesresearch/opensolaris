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
 * Multiboot2 entry for the amd64pc bootstrap; the modern counterpart of
 * i86pc/dboot/dboot_grub.s.  GRUB 2 (BIOS or UEFI) loads the ELF image at
 * its physical addresses and enters _start in 32-bit protected mode with
 * paging off, %eax = MULTIBOOT2_BOOTLOADER_MAGIC and %ebx = the physical
 * address of the boot information structure.
 *
 * This code clears BSS, checks for long mode, identity maps the low
 * 4 GiB with 2 MiB pages, enables PAE + long mode + paging and enters
 * amd64pc_main(magic, mbi) in 64-bit mode.  The page tables built here
 * are replaced by page_t-backed 4 KiB tables for everything the HAT maps
 * later; this identity map stays as the kernel's own mapping.
 */

#define	_ASM
#include <sys/controlregs.h>

#define	MB2_HEADER_MAGIC	0xe85250d6
#define	MB2_ARCH_I386		0
#define	MB2_TAG_END		0
#define	MB2_TAG_FRAMEBUFFER	5
#define	MB2_TAG_OPTIONAL	1

#define	PTE_P			0x001
#define	PTE_RW			0x002
#define	PTE_PS			0x080

#define	COM1			0x3f8

	.section .multiboot2, "a"
	.p2align 3
mb2_header:
	.long	MB2_HEADER_MAGIC
	.long	MB2_ARCH_I386
	.long	mb2_header_end - mb2_header
	.long	0x100000000 - (MB2_HEADER_MAGIC + MB2_ARCH_I386 + \
		    (mb2_header_end - mb2_header))

	/*
	 * Ask for a linear framebuffer (optional, no preferred geometry) so
	 * that UEFI systems without a serial port still get a console.  On
	 * BIOS GRUB may leave the adapter in EGA text mode; both are handled.
	 */
	.p2align 3
	.short	MB2_TAG_FRAMEBUFFER
	.short	MB2_TAG_OPTIONAL
	.long	20
	.long	0, 0, 32

	.p2align 3
	.short	MB2_TAG_END
	.short	0
	.long	8
mb2_header_end:

	.section .text.boot, "ax"
	.code32
	.globl	_start
_start:
	cli
	cld
	movl	%eax, %esi
	movl	%ebx, %ebp

	/* Clear BSS: the page tables and the boot stack live there. */
	movl	$__bss_start, %edi
	movl	$__bss_end, %ecx
	subl	%edi, %ecx
	xorl	%eax, %eax
	rep stosb

	movl	%esi, mb2_magic
	movl	%ebp, mb2_info
	movl	$boot_stack_top, %esp

	/* Long mode is CPUID 0x80000001 %edx bit 29. */
	movl	$0x80000000, %eax
	cpuid
	cmpl	$0x80000001, %eax
	jb	.Lno_long_mode
	movl	$0x80000001, %eax
	cpuid
	testl	$(1 << 29), %edx
	jz	.Lno_long_mode

	/* PML4[0] -> PDPT; PDPT[0..3] -> four page directories. */
	movl	$boot_pdpt, %eax
	orl	$(PTE_P | PTE_RW), %eax
	movl	%eax, boot_pml4

	movl	$boot_pd, %eax
	orl	$(PTE_P | PTE_RW), %eax
	xorl	%ecx, %ecx
1:	movl	%eax, boot_pdpt(, %ecx, 8)
	addl	$4096, %eax
	incl	%ecx
	cmpl	$4, %ecx
	jb	1b

	/* 2048 x 2 MiB identity mappings: physical 0 - 4 GiB. */
	xorl	%ecx, %ecx
2:	movl	%ecx, %eax
	shll	$21, %eax
	orl	$(PTE_P | PTE_RW | PTE_PS), %eax
	movl	%eax, boot_pd(, %ecx, 8)
	incl	%ecx
	cmpl	$2048, %ecx
	jb	2b

	movl	%cr4, %eax
	orl	$(CR4_PAE | CR4_PGE), %eax
	movl	%eax, %cr4

	movl	$boot_pml4, %eax
	movl	%eax, %cr3

	movl	$MSR_AMD_EFER, %ecx
	rdmsr
	orl	$AMD_EFER_LME, %eax
	wrmsr

	movl	%cr0, %eax
	orl	$(CR0_PG | CR0_WP | CR0_PE), %eax
	movl	%eax, %cr0

	lgdt	boot_gdtr
	ljmp	$0x08, $.Llong_mode

.Lno_long_mode:
	movl	$.Lno_lm_msg, %esi
3:	movb	(%esi), %al
	testb	%al, %al
	jz	4f
	movw	$(COM1 + 5), %dx
5:	inb	%dx, %al
	testb	$0x20, %al
	jz	5b
	movb	(%esi), %al
	movw	$COM1, %dx
	outb	%al, %dx
	incl	%esi
	jmp	3b
4:	hlt
	jmp	4b

	.code64
.Llong_mode:
	movw	$0x10, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %ss
	xorl	%eax, %eax
	movw	%ax, %fs
	movw	%ax, %gs
	movq	$boot_stack_top, %rsp
	xorl	%ebp, %ebp
	movl	mb2_magic(%rip), %edi
	movl	mb2_info(%rip), %esi
	call	amd64pc_main
6:	cli
	hlt
	jmp	6b

	.section .rodata
.Lno_lm_msg:
	.asciz	"amd64pc: this CPU does not support 64-bit long mode\r\n"

	.p2align 3
boot_gdt:
	.quad	0
	.quad	0x00af9a000000ffff	/* 0x08: 64-bit ring 0 code */
	.quad	0x00cf92000000ffff	/* 0x10: ring 0 data */
boot_gdt_end:

	.p2align 2
boot_gdtr:
	.short	boot_gdt_end - boot_gdt - 1
	.long	boot_gdt

	.data
	.p2align 2
	.globl	mb2_magic, mb2_info
mb2_magic:
	.long	0
mb2_info:
	.long	0

	.bss
	.p2align 12
	.globl	boot_pml4
boot_pml4:
	.space	4096
boot_pdpt:
	.space	4096
	.globl	boot_pd
boot_pd:
	.space	4 * 4096
	.p2align 4
boot_stack:
	.space	16384
	.globl	boot_stack_top
boot_stack_top:
