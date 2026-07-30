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
 * Machine-level setup — called from locore.s before main().
 * Parallel to i86pc/os/mlsetup.c.
 */

#include <sys/types.h>
#include <sys/platform.h>
#include <sys/fdt.h>
#include <sys/psci.h>

/* Forward declarations */
extern void startup_init(void);
extern void pl011_init(uint64_t);
extern void pl011_puts(const char *);
extern void gicv3_init(void);
extern void gicv3_set_base(uint64_t, uint64_t);
extern void gicv3_cpu_init(void);
extern void arch_timer_init(void);
extern void arm64_boot_cpu_init(void);
extern uint64_t fdt_phys_addr;

struct arm64_platform arm64_platform;

/*
 * mlsetup() — machine-level setup.
 * Called from locore.s with a valid stack, MMU on.
 *
 * This initializes the minimum platform state needed for main() to run:
 * - CPU identification
 * - Interrupt controller (GICv3)
 * - Timer
 * - Boot console
 */
void
mlsetup(void)
{
	uint64_t midr, mpidr;

	/*
	 * Read CPU identification registers.
	 */
	__asm__ __volatile__("mrs %0, midr_el1" : "=r"(midr));
	__asm__ __volatile__("mrs %0, mpidr_el1" : "=r"(mpidr));

	/*
	 * TODO: Decode MIDR fields:
	 *   Implementer [31:24], Variant [23:20], Architecture [19:16],
	 *   PartNum [15:4], Revision [3:0]
	 */

	/* Discover the platform before switching away from the fixed boot UART. */
	if (arm64_fdt_parse(fdt_phys_addr, &arm64_platform) != 0) {
		pl011_init(ARM64PC_UART_BASE);
		pl011_puts("mlsetup: invalid or unsupported FDT\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}

	/* Console first: every later failure must be diagnosable. */
	pl011_init(arm64_platform.uart_base);
	pl011_puts("mlsetup: PL011 console initialized\n");

	/* Establish curthread/CPU before entering any common kernel code. */
	arm64_boot_cpu_init();

	psci_conduit = arm64_platform.psci_conduit;
	gicv3_set_base(arm64_platform.gicd_base, arm64_platform.gicr_base);

	/* Keep interrupts masked until the common kernel owns dispatch. */
	gicv3_init();
	gicv3_cpu_init();
	arch_timer_init();

	startup_init();

	/*
	 * TODO: Additional mlsetup tasks:
	 * - Initialize PSCI interface
	 * - Set up boot memory allocator
	 * - Initialize HAT
	 */
}
