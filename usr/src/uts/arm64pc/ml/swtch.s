/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/asm_linkage.h>
#include "assym.h"

	.text
	.align	2

	/*
	 * Low-level kthread context transfer.  The caller owns dispatcher locks and
	 * address-space selection.  This routine saves/restores t_pcb and publishes
	 * the incoming thread through cpu_thread and TPIDR_EL1 before it runs.
	 */
	ENTRY(arm64_thread_switch)
	add	x2, x0, #T_PCB
	str	x30, [x2, #0]
	mov	x3, sp
	str	x3, [x2, #8]
	stp	x19, x20, [x2, #16]
	stp	x21, x22, [x2, #32]
	stp	x23, x24, [x2, #48]
	stp	x25, x26, [x2, #64]
	stp	x27, x28, [x2, #80]
	stp	x29, x30, [x2, #96]

	add	x4, x1, #T_PCB
	ldr	x2, [x4, #0]
	ldr	x3, [x4, #8]
	ldr	x5, [x1, #T_CPU]
	cbz	x5, 1f
	str	x1, [x5, #CPU_THREAD]
1:
	msr	tpidr_el1, x1
	isb
	mov	sp, x3
	ldp	x19, x20, [x4, #16]
	ldp	x21, x22, [x4, #32]
	ldp	x23, x24, [x4, #48]
	ldp	x25, x26, [x4, #64]
	ldp	x27, x28, [x4, #80]
	ldp	x29, x30, [x4, #96]
	br	x2
	SET_SIZE(arm64_thread_switch)

	/*
	 * Common dispatcher context switch.
	 *
	 * The outgoing thread cannot be unlocked while this CPU is still using
	 * its stack: another CPU could immediately resume it.  Move to the CPU's
	 * idle stack first, publish the idle thread, and only then release the
	 * outgoing resume lock.  The incoming thread is acquired before its
	 * saved stack and register state become active.
	 */
	ENTRY(resume)
	mrs	x9, tpidr_el1
	add	x10, x9, #T_PCB
	adr	x11, .Lresume_return
	str	x11, [x10, #0]
	mov	x11, sp
	str	x11, [x10, #8]
	stp	x19, x20, [x10, #16]
	stp	x21, x22, [x10, #32]
	stp	x23, x24, [x10, #48]
	stp	x25, x26, [x10, #64]
	stp	x27, x28, [x10, #80]
	stp	x29, x30, [x10, #96]

	ldr	x10, [x9, #T_CPU]
	ldr	x11, [x10, #CPU_IDLE_THREAD]
	ldr	x12, [x11, #(T_PCB + 8)]
	mov	sp, x12
	str	x11, [x10, #CPU_THREAD]
	msr	tpidr_el1, x11
	isb

	/*
	 * Release semantics make all outgoing state visible before another CPU
	 * acquires this thread.  SEV wakes contenders sleeping in WFE.
	 */
	add	x13, x9, #T_LOCK
	stlrb	wzr, [x13]
	sev

	add	x13, x0, #T_LOCK
.Lresume_lock:
	ldaxrb	w11, [x13]
	cbnz	w11, .Lresume_wait
	mov	w11, #0xff
	stxrb	w12, w11, [x13]
	cbnz	w12, .Lresume_lock
	b	.Lresume_locked
.Lresume_wait:
	wfe
	b	.Lresume_lock

.Lresume_locked:
	str	x10, [x0, #T_CPU]
	ldr	x11, [x0, #T_LWP]
	str	x11, [x10, #CPU_LWP]
	str	x0, [x10, #CPU_THREAD]
	dmb	ish
	msr	tpidr_el1, x0
	isb

	add	x4, x0, #T_PCB
	ldr	x2, [x4, #0]
	ldr	x3, [x4, #8]
	mov	sp, x3
	ldp	x19, x20, [x4, #16]
	ldp	x21, x22, [x4, #32]
	ldp	x23, x24, [x4, #48]
	ldp	x25, x26, [x4, #64]
	ldp	x27, x28, [x4, #80]
	ldp	x29, x30, [x4, #96]

	/*
	 * spl0() is a normal C call and may clobber x0-x18.  Preserve both the
	 * saved continuation and the restored link register on the new stack.
	 */
	stp	x2, x30, [sp, #-16]!
	bl	spl0
	ldp	x2, x30, [sp], #16
	br	x2

.Lresume_return:
	ret
	SET_SIZE(resume)
