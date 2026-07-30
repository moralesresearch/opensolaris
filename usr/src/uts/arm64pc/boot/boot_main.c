/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 *
 * Freestanding bootstrap milestone.  This is intentionally separate from
 * uts/common/os/main.c: it proves the architecture entry, MMU, exception,
 * console, timer, interrupt-controller and firmware interfaces before the
 * common kernel's VM, thread and device frameworks are linked in.
 */

#include <sys/types.h>
#include <sys/privregs.h>
#include <sys/platform.h>
#include <sys/fdt.h>
#include <sys/bootmem.h>
#include <sys/boot_hat.h>
#include <sys/boot_page.h>
#include <sys/intr.h>
#include <sys/smp.h>
#include <sys/usermode.h>

extern uint64_t fdt_phys_addr;
extern void pl011_puts(const char *);
extern uint64_t arch_timer_get_freq(void);
extern uint64_t arch_timer_gethrtime(void);
extern void arch_timer_start(uint64_t);
extern void gicv3_irq_enable(uint32_t);
extern uint32_t psci_version(void);
extern volatile uint64_t arm64_timer_ticks;
extern struct arm64_platform arm64_platform;
extern int arm64_context_selftest(void);
extern int arm64_switch_selftest(void);
extern int arm64_thread_selftest(void);
extern int arm64_resume_selftest(void);
extern int arm64_hat_selftest(void);
extern int arm64_user_hat_selftest(void);
extern int arm64_page_selftest(void);
extern int arm64_reloc_selftest(void);
extern int arm64_common_selftest(void);
extern int arm64_lock_selftest(void);
extern int arm64_spl_selftest(void);
extern int arm64_boot_cpu_selftest(void);

static void
puthex(uint64_t value)
{
	static const char digits[] = "0123456789abcdef";
	char out[19];
	int i;

	out[0] = '0';
	out[1] = 'x';
	for (i = 0; i != 16; i++)
		out[i + 2] = digits[(value >> (60 - i * 4)) & 0xf];
	out[18] = '\0';
	pl011_puts(out);
}

void
arm64_bootstrap_main(void)
{
	uint32_t version;
	int cpu_test;

	pl011_puts("bootstrap: exception vectors installed\n");
	pl011_puts("bootstrap: GICv3 CPU interface initialized\n");
	cpu_test = arm64_boot_cpu_selftest();
	if (cpu_test != 0) {
		pl011_puts("bootstrap: boot CPU/thread ABI test failed ");
		puthex((uint64_t)(int64_t)cpu_test);
		pl011_puts("\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: t0/CPU/TPIDR_EL1 CPU_ABI_OK\n");
	pl011_puts("bootstrap: FDT at ");
	puthex(fdt_phys_addr);
	if (arm64_fdt_validate(fdt_phys_addr) != 0) {
		pl011_puts(" INVALID\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts(" size ");
	puthex(arm64_fdt_size(fdt_phys_addr));
	pl011_puts(" FDT_OK");
	pl011_puts("\nbootstrap: RAM ");
	puthex(arm64_platform.ram_base);
	pl011_puts(" + ");
	puthex(arm64_platform.ram_size);
	pl011_puts(" UART ");
	puthex(arm64_platform.uart_base);
	pl011_puts(" CPUs ");
	puthex(arm64_platform.cpu_count);
	pl011_puts("\nbootstrap: current EL ");
	puthex(arm64_current_el());
	pl011_puts("\nbootstrap: timer frequency ");
	puthex(arch_timer_get_freq());
	pl011_puts(" Hz\nbootstrap: counter ");
	puthex(arch_timer_gethrtime());
	pl011_puts(" ns\n");

	version = psci_version();
	pl011_puts("bootstrap: PSCI version ");
	puthex(version);
	pl011_puts("\n");
	arm64_intr_bootstrap_init();
	if (arm64_intr_selftest() != 0) {
		pl011_puts("bootstrap: interrupt registration test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: shared interrupt dispatcher AUTOVECT_OK\n");

	if (arm64_bootmem_init(&arm64_platform, fdt_phys_addr,
	    arm64_fdt_size(fdt_phys_addr)) != 0 ||
	    arm64_boot_hat_init(&arm64_platform) != 0) {
		pl011_puts("bootstrap: boot HAT initialization failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	{
		void (*high_probe)(void) = (void (*)(void))(uintptr_t)
		    arm64_boot_phys_to_kva(
		    (uint64_t)(uintptr_t)arm64_boot_high_probe);
		high_probe();
	}
	pl011_puts("bootstrap: TTBR1 root ");
	puthex(arm64_boot_hat_root());
	pl011_puts(" bootmem used ");
	puthex(arm64_bootmem_used());
	pl011_puts("\n");
	if (arm64_page_init(&arm64_platform, fdt_phys_addr,
	    arm64_fdt_size(fdt_phys_addr)) != 0) {
		pl011_puts("bootstrap: common page_t freelist init failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: common page_t pages ");
	puthex(arm64_page_total());
	pl011_puts(" free ");
	puthex(arm64_page_freecnt());
	pl011_puts(" PAGE_INIT_OK\n");
	if (arm64_hat_selftest() != 0) {
		pl011_puts("bootstrap: page-granular HAT test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: page map/lookup/unmap PAGE_HAT_OK\n");
	if (arm64_page_selftest() != 0) {
		pl011_puts("bootstrap: page_t mapping-list test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: page_t freelist/mapping list/attributes "
	    "PAGE_OK\n");

	if (arm64_smp_start(&arm64_platform) != 0) {
		pl011_puts("bootstrap: secondary CPU startup failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: online CPUs ");
	puthex(arm64_online_cpus);
	pl011_puts(" SMP_OK\n");
	if (arm64_smp_test_cpu_abi(&arm64_platform) != 0) {
		pl011_puts("bootstrap: per-CPU thread ABI test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: per-CPU curthread state SMP_CPU_ABI_OK\n");
	if (arm64_smp_test_ipi(&arm64_platform) != 0) {
		pl011_puts("bootstrap: interprocessor interrupt failed count ");
		puthex(arm64_ipi_count);
		pl011_puts(" last IRQ ");
		puthex(arm64_last_irq);
		pl011_puts("\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: interprocessor interrupt IPI_OK\n");
	if (arm64_smp_test_xcall(&arm64_platform) != 0) {
		pl011_puts("bootstrap: synchronous cross-call failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: synchronous cross-call XCALL_OK\n");
	if (arm64_lock_selftest() != 0) {
		pl011_puts("bootstrap: cross-CPU lock primitive failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: cross-CPU dispatcher lock LOCK_OK\n");
	if (arm64_spl_selftest() != 0) {
		pl011_puts("bootstrap: GIC priority/PIL test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: GIC priority mask/PIL SPL_OK\n");
	if (arm64_user_hat_selftest() != 0) {
		pl011_puts("bootstrap: user address-space HAT test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: TTBR0 ASID and TLB shootdown USER_HAT_OK\n");
	if (arm64_el0_selftest() != 0) {
		pl011_puts("bootstrap: EL0 syscall transition failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: EL0 eret/SVC/EL1 return SYSCALL_OK\n");
	if (arm64_el0_fault_selftest() != 0) {
		pl011_puts("bootstrap: recoverable EL0 page fault failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: recoverable EL0 translation fault PAGEFAULT_OK\n");
	if (arm64_context_selftest() != 0) {
		pl011_puts("bootstrap: saved context test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: setjmp/longjmp CONTEXT_OK\n");
	if (arm64_switch_selftest() != 0) {
		pl011_puts("bootstrap: context switch test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: cooperative context switch SWITCH_OK\n");
	if (arm64_thread_selftest() != 0) {
		pl011_puts("bootstrap: kthread switch test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: generated-offset kthread switch KTHREAD_OK\n");
	if (arm64_resume_selftest() != 0) {
		pl011_puts("bootstrap: common dispatcher resume test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: common dispatcher resume ABI DISP_RESUME_OK\n");
	if (arm64_reloc_selftest() != 0) {
		pl011_puts("bootstrap: krtld relocation test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: AArch64 krtld relocations RELOC_OK\n");
	if (arm64_common_selftest() != 0) {
		pl011_puts("bootstrap: common kernel object test failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	pl011_puts("bootstrap: linked uts/common genunix object COMMON_OK\n");

	/* Prove the vector table, GIC PPI path and physical timer end-to-end. */
	gicv3_irq_enable(ARM64PC_TIMER_IRQ);
	arch_timer_start(10000);
	arm64_intr_enable();
	while (arm64_timer_ticks == 0)
		__asm__ __volatile__("wfi");
	arm64_intr_disable();
	pl011_puts("bootstrap: timer interrupt IRQ_OK\n");

	pl011_puts("OpenSolaris/AArch64 BOOT_OK\n");
	pl011_puts("bootstrap: common kernel handoff is the next milestone\n");

	for (;;)
		__asm__ __volatile__("wfi");
}
