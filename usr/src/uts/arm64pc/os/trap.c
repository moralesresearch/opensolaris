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
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 *
 * AArch64 trap/exception handler.
 * Parallel to i86pc/os/trap.c.
 *
 * Called from the exception vector stubs in exception_vectors.s.
 */

#include <sys/types.h>
#include <sys/regset.h>
#include <sys/controlregs.h>
#include <sys/gicv3.h>
#include <sys/intr.h>
#include <sys/trap.h>
#include <sys/usermode.h>

/* Forward declarations */
extern void pl011_puts(const char *);
extern void gicv3_irq_complete(uint32_t);
extern uint32_t gicv3_irq_acknowledge(void);
void arm64_trap_fatal(struct regs *, int);

volatile uint64_t arm64_timer_ticks;
volatile uint64_t arm64_last_irq = GIC_SPURIOUS_IRQ;
arm64_pagefault_func_t arm64_pagefault_dispatch;

/*
 * Print a hex number to the console (early debug).
 */
static void
puthex(uint64_t val)
{
	static const char hex[] = "0123456789abcdef";
	char buf[19];
	int i;

	buf[0] = '0';
	buf[1] = 'x';
	for (i = 0; i < 16; i++)
		buf[2 + i] = hex[(val >> (60 - i * 4)) & 0xf];
	buf[18] = '\0';
	pl011_puts(buf);
}

/*
 * arm64_trap() — synchronous exception handler.
 * Called from exception_vectors.s with:
 *   x0 = pointer to trap frame (struct regs)
 *   x1 = trap type (from vector table position)
 */
void
arm64_trap(struct regs *rp, int traptype)
{
	uint64_t esr = rp->r_esr;
	uint32_t ec = (esr >> ESR_EC_SHIFT) & 0x3f;
	uint64_t far = rp->r_far;

	switch (ec) {
	case ESR_EC_SVC64:
		if (traptype != T_SYNC_EL0_64) {
			arm64_trap_fatal(rp, traptype);
			break;
		}
		if ((esr & 0xffff) == 0) {
			/* AArch64 ABI: syscall number x8, arguments x0-x5. */
			switch (rp->r_x8) {
			case 1:
				rp->r_x0 += rp->r_x1;
				break;
			default:
				rp->r_x0 = (uint64_t)-1;
				break;
			}
#ifdef ARM64_BOOTSTRAP
		} else if ((esr & 0xffff) == 1) {
			/* Controlled return used only by the EL0 bootstrap test. */
			rp->r_pc = (uint64_t)(uintptr_t)arm64_el0_resume;
			rp->r_pstate = SPSR_MODE_EL1h | SPSR_DAIF;
#endif
		} else {
			rp->r_x0 = (uint64_t)-1;
		}
		break;

	case ESR_EC_DABT_LOW:
	case ESR_EC_DABT_CUR:
		/*
		 * Data abort — page fault.
		 * FAR_EL1 has the faulting address.
		 * ISS[5:0] has the fault status code.
		 */
		if (traptype == T_SYNC_EL0_64 && arm64_pagefault_dispatch != 0 &&
		    arm64_pagefault_dispatch(far,
		    ((esr & ESR_ISS_DFSC_MASK) >= FSC_PERM_L1 &&
		    (esr & ESR_ISS_DFSC_MASK) <= FSC_PERM_L3) ? 1 : 0,
		    (esr & ESR_ISS_WnR) != 0) == 0)
			break;
		pl011_puts("Data abort at ");
		puthex(far);
		pl011_puts(", ESR=");
		puthex(esr);
		pl011_puts(", PC=");
		puthex(rp->r_pc);
		pl011_puts("\n");

		/*
		 * TODO: Call VM fault handler.
		 * For now, fall through to fatal.
		 */
		arm64_trap_fatal(rp, traptype);
		break;

	case ESR_EC_IABT_LOW:
	case ESR_EC_IABT_CUR:
		/*
		 * Instruction abort.
		 */
		pl011_puts("Instruction abort at ");
		puthex(far);
		pl011_puts("\n");
		arm64_trap_fatal(rp, traptype);
		break;

	case ESR_EC_BRK:
		/*
		 * BRK instruction — software breakpoint.
		 * TODO: kmdb/DTrace integration.
		 */
		pl011_puts("BRK at ");
		puthex(rp->r_pc);
		pl011_puts("\n");
		/* Skip the BRK instruction (4 bytes) */
		rp->r_pc += 4;
		break;

	case ESR_EC_FP_ASIMD:
		/*
		 * FP/NEON access trap.
		 * TODO: Lazy FPU context switching.
		 */
		pl011_puts("FP trap\n");
		break;

	case ESR_EC_UNKNOWN:
	default:
		pl011_puts("Unknown exception EC=");
		puthex(ec);
		pl011_puts(" ESR=");
		puthex(esr);
		pl011_puts("\n");
		arm64_trap_fatal(rp, traptype);
		break;
	}
}

/*
 * arm64_trap_fatal() — unrecoverable exception.
 */
void
arm64_trap_fatal(struct regs *rp, int traptype)
{
	pl011_puts("\n*** FATAL EXCEPTION ***\n");
	pl011_puts("Trap type: ");
	puthex(traptype);
	pl011_puts("\n");

	if (rp) {
		pl011_puts("  PC:      ");
		puthex(rp->r_pc);
		pl011_puts("\n  SP:      ");
		puthex(rp->r_sp);
		pl011_puts("\n  ESR:     ");
		puthex(rp->r_esr);
		pl011_puts("\n  FAR:     ");
		puthex(rp->r_far);
		pl011_puts("\n  PSTATE:  ");
		puthex(rp->r_pstate);
		pl011_puts("\n  LR(x30): ");
		puthex(rp->r_x30);
		pl011_puts("\n  FP(x29): ");
		puthex(rp->r_x29);
		pl011_puts("\n");
	}

	pl011_puts("System halted.\n");
	for (;;)
		__asm__ __volatile__("wfi");
}

/*
 * arm64_irq_handler() — IRQ exception handler.
 * Called from exception_vectors.s for IRQ exceptions.
 */
void
arm64_irq_handler(struct regs *rp)
{
	uint32_t irq;

	/* Acknowledge the interrupt */
	irq = gicv3_irq_acknowledge();
	arm64_last_irq = irq;

	if (irq == 1023) {
		/* Spurious interrupt */
		return;
	}

	(void) arm64_intr_dispatch(irq);

	/* Signal end of interrupt */
	gicv3_irq_complete(irq);
}

/*
 * arm64_fiq_handler() — FIQ exception handler.
 * FIQs are typically used for secure interrupts; not expected in NS kernel.
 */
void
arm64_fiq_handler(struct regs *rp)
{
	pl011_puts("Unexpected FIQ\n");
}
