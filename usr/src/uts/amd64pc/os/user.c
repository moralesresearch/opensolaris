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
 * Ring-3 harness: map a user text and stack page, drop to user mode
 * through iretq on UCS_SEL/UDS_SEL, take a round-trip SYSCALL/SYSRET
 * system call, exit through a second system call, and then run a user
 * program that faults on an unmapped page so the #PF arrives on the TSS
 * rsp0 stack with a user-mode error code.
 */

#include <sys/types.h>
#include <sys/regset.h>
#include <sys/privregs.h>
#include <sys/segments.h>
#include <sys/trap.h>
#include <vm/page.h>
#include <sys/amd64pc.h>

#define	USER_TEXT_VA	0x0000008000000000ULL
#define	USER_STACK_VA	0x0000008000100000ULL
#define	USER_FAULT_VA	0x0000008000400000ULL
#define	USER_TEST_ARG	0x0505a1a0d15c0ULL

#define	PT_WRITABLE	0x002ULL
#define	PT_USER		0x004ULL
#define	PT_NX		(1ULL << 63)


#define	SYS_test	1
#define	SYS_exit	2

extern char amd64pc_user_prog[], amd64pc_user_prog_end[];
extern char amd64pc_user_fault[], amd64pc_user_fault_end[];
extern uint64_t amd64pc_syscall_kstack;
extern void amd64pc_user_enter(uint64_t, uint64_t, uint64_t, uint64_t)
    __attribute__((noreturn));
extern void (*trap_user_hook)(struct regs *);

static uint8_t user_kstack[16384] __attribute__((aligned(16)));
static amd64pc_jmpbuf_t user_jb;
static volatile uint64_t sys_test_arg, sys_test_rsp, sys_exit_code;
static volatile uint64_t fault_trapno, fault_err, fault_addr, fault_cs;

uint64_t
amd64pc_syscall(uint64_t num, uint64_t a0, uint64_t a1)
{
	switch (num) {
	case SYS_test:
		sys_test_arg = a0;
		sys_test_rsp = a1;
		return (a0 + 1);
	case SYS_exit:
		sys_exit_code = a0;
		amd64pc_longjmp(&user_jb, 1);
	default:
		return ((uint64_t)-1);
	}
}

static void
user_trap(struct regs *rp)
{
	uint64_t cr2;

	__asm__ __volatile__("movq %%cr2, %0" : "=r"(cr2));
	fault_trapno = rp->r_trapno;
	fault_err = rp->r_err;
	fault_cs = rp->r_cs;
	fault_addr = cr2;
	amd64pc_longjmp(&user_jb, 2);
}

static int
load_text(page_t *pp, const char *start, const char *end)
{
	uint8_t *dst = (uint8_t *)(uintptr_t)page_boot_pa(pp);
	const char *s;

	if (end - start > 4096)
		return (-1);
	for (s = start; s < end; s++)
		*dst++ = (uint8_t)*s;
	return (0);
}

static int
run_user(void)
{
	int rv = amd64pc_setjmp(&user_jb);

	if (rv == 0)
		amd64pc_user_enter(USER_TEXT_VA, USER_STACK_VA + 4096,
		    UCS_SEL, UDS_SEL);
	return (rv);
}

int
user_selftest(void)
{
	page_t *text, *stack;
	uint64_t ktop = (uint64_t)(uintptr_t)&user_kstack[sizeof (user_kstack)];
	int rv = 0;

	text = page_boot_alloc();
	stack = page_boot_alloc();
	if (text == 0 || stack == 0)
		return (-1);
	desctbl_set_rsp0(0, ktop);
	amd64pc_syscall_kstack = ktop;
	trap_user_hook = user_trap;

	/* 1: SYSCALL round trip and exit. */
	if (load_text(text, amd64pc_user_prog, amd64pc_user_prog_end) != 0 ||
	    hat_boot_map(USER_TEXT_VA, page_boot_pa(text), PT_USER) != 0 ||
	    hat_boot_map(USER_STACK_VA, page_boot_pa(stack),
	    PT_USER | PT_WRITABLE | PT_NX) != 0) {
		rv = -2;
		goto out;
	}
	if (run_user() != 1 || sys_test_arg != USER_TEST_ARG ||
	    sys_exit_code != USER_TEST_ARG + 1 ||
	    sys_test_rsp != USER_STACK_VA + 4096) {
		rv = -3;
		goto out;
	}
	cons_puts("amd64pc: ring 3 SYSCALL/SYSRET round trip SYSCALL_OK\n");

	/* 2: user page fault delivered on the rsp0 stack. */
	if (hat_boot_unmap(USER_TEXT_VA) != 0 ||
	    load_text(text, amd64pc_user_fault, amd64pc_user_fault_end) != 0 ||
	    hat_boot_map(USER_TEXT_VA, page_boot_pa(text), PT_USER) != 0) {
		rv = -4;
		goto out;
	}
	if (run_user() != 2 || fault_trapno != T_PGFLT ||
	    fault_addr != USER_FAULT_VA || (fault_err & PF_ERR_USER) == 0 ||
	    (fault_cs & 3) != SEL_UPL) {
		rv = -5;
		goto out;
	}
	cons_puts("amd64pc: recoverable ring 3 page fault PAGEFAULT_OK\n");

out:
	trap_user_hook = 0;
	(void) hat_boot_unmap(USER_TEXT_VA);
	(void) hat_boot_unmap(USER_STACK_VA);
	page_boot_free(text);
	page_boot_free(stack);
	return (rv);
}
