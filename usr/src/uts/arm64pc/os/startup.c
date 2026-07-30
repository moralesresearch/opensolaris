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
 * Kernel startup for AArch64 platform.
 * Parallel to i86pc/os/startup.c.
 */

#include <sys/types.h>
#include <sys/platform.h>

extern void pl011_puts(const char *);
extern void psci_system_off(void);

/*
 * Initial kernel stack (used by BSP before thread 0 stack is set up).
 * 16KB, aligned to 16 bytes.
 */
char t0stack[16384] __attribute__((aligned(16)));

/*
 * Boot banner strings.
 */
static const char *boot_banner =
    "OpenSolaris AArch64 Kernel\n"
    "Copyright 2026 OpenSolaris Contributors\n";

/*
 * Early console output (PL011 UART at QEMU virt address).
 * TODO: Move to proper console driver after DDI init.
 */
static void
early_puts(const char *s)
{
	pl011_puts(s);
}

/*
 * startup_init() — called from mlsetup() to perform platform-specific
 * initialization before main().
 */
void
startup_init(void)
{
	early_puts(boot_banner);
	early_puts("startup_init: initializing platform...\n");

	/*
	 * TODO: Initialize in order:
	 * 1. Parse FDT/ACPI for memory map
	 * 2. Initialize HAT (page table management)
	 * 3. Initialize GICv3
	 * 4. Initialize generic timer
	 * 5. Set up CPU data structures
	 * 6. Initialize VM subsystem
	 */

	early_puts("startup_init: done.\n");
}

/*
 * startup_end() — final startup tasks after main() subsystem init.
 */
void
startup_end(void)
{
	early_puts("startup_end: kernel initialization complete.\n");
}

/*
 * mach_cpu_idle() — idle the CPU.
 * Called by the idle thread. Uses WFI (Wait For Interrupt).
 */
void
mach_cpu_idle(void)
{
	__asm__ __volatile__("wfi" ::: "memory");
}

/*
 * mach_cpu_halt() — halt the system.
 */
void
mach_cpu_halt(const char *msg)
{
	if (msg)
		early_puts(msg);
	early_puts("\nSystem halted.\n");

	psci_system_off();
}
