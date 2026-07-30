/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#include <sys/types.h>
#include <sys/controlregs.h>
#include <sys/privregs.h>
#include <sys/machlock.h>
#include <sys/bootmem.h>
#include <sys/gicv3.h>
#include <sys/psci.h>
#include <sys/smp.h>

#define	AP_STACK_SIZE	16384
#define	MPIDR_AFFINITY_MASK	0xff00ffffffULL

extern uint64_t ap_stacks[];
extern void secondary_start(void);
extern void pl011_puts(const char *);
extern uint64_t arch_timer_gethrtime(void);
extern int arm64_boot_cpu_init_secondary(uint32_t);
extern int arm64_boot_cpu_selftest_id(uint32_t);

volatile uint64_t arm64_online_cpus = 1;
volatile uint64_t arm64_ipi_count;

#define	ARM64_XCALL_SGI	1

struct arm64_xcall_mailbox {
	volatile uint32_t pending;
	volatile uint32_t done;
	xc_data_t data;
	volatile int result;
};

static struct arm64_xcall_mailbox arm64_xcall_mbox[ARM64_FDT_MAX_CPUS];
static uint64_t arm64_cpu_mpidr[ARM64_FDT_MAX_CPUS];
static uint32_t arm64_cpu_count;

uint32_t
arm64_smp_current_cpuid(void)
{
	uint64_t mpidr = read_sysreg(mpidr_el1) & MPIDR_AFFINITY_MASK;
	uint32_t cpu;

	for (cpu = 0; cpu < arm64_cpu_count; cpu++) {
		if (arm64_cpu_mpidr[cpu] == mpidr)
			return (cpu);
	}
	return (ARM64_FDT_MAX_CPUS);
}

int
arm64_smp_start(const struct arm64_platform *platform)
{
	uint64_t boot_mpidr = read_sysreg(mpidr_el1) & MPIDR_AFFINITY_MASK;
	uint32_t cpu;

	if (platform == 0 || platform->cpu_count == 0)
		return (-1);
	arm64_cpu_count = platform->cpu_count;
	for (cpu = 0; cpu < platform->cpu_count; cpu++)
		arm64_cpu_mpidr[cpu] = platform->cpu_mpidr[cpu];
	dsb(sy);
	for (cpu = 0; cpu < platform->cpu_count; cpu++) {
		uint64_t stack, deadline, expected;
		int result;

		if (platform->cpu_mpidr[cpu] == boot_mpidr)
			continue;
		stack = (uint64_t)(uintptr_t)
		    arm64_bootmem_alloc(AP_STACK_SIZE, 16);
		if (stack == 0)
			return (-1);
		ap_stacks[cpu] = stack + AP_STACK_SIZE;
		dsb(sy);
		expected = arm64_online_cpus + 1;

		result = psci_cpu_on_context(platform->cpu_mpidr[cpu],
		    (uint64_t)(uintptr_t)secondary_start, cpu);
		if (result != PSCI_SUCCESS)
			return (result);

		deadline = arch_timer_gethrtime() + 1000000000ULL;
		while (arm64_online_cpus < expected &&
		    arch_timer_gethrtime() < deadline)
			SMT_PAUSE();
		if (arm64_online_cpus < expected)
			return (-1);
	}
	return (0);
}

void
arm64_smp_ipi_handler(void)
{
	uint32_t cpu = arm64_smp_current_cpuid();
	struct arm64_xcall_mailbox *mbox;

	(void) arm64_atomic_add_64(&arm64_ipi_count, 1);
	if (cpu >= ARM64_FDT_MAX_CPUS)
		return;
	mbox = &arm64_xcall_mbox[cpu];
	dmb(ish);
	if (mbox->pending == 0)
		return;
	mbox->result = mbox->data.xc_func(mbox->data.xc_a1,
	    mbox->data.xc_a2, mbox->data.xc_a3);
	dmb(ish);
	mbox->pending = 0;
	mbox->done = 1;
	dsb(ish);
	__asm__ __volatile__("sev");
}

int
arm64_xcall_sync(const struct arm64_platform *platform, xc_func_t func,
    xc_arg_t a1, xc_arg_t a2, xc_arg_t a3, int *results)
{
	uint64_t boot_mpidr = read_sysreg(mpidr_el1) & MPIDR_AFFINITY_MASK;
	uint64_t deadline;
	uint32_t cpu;

	if (platform == 0 || func == 0)
		return (-1);
	for (cpu = 0; cpu < platform->cpu_count; cpu++) {
		struct arm64_xcall_mailbox *mbox = &arm64_xcall_mbox[cpu];

		if (platform->cpu_mpidr[cpu] == boot_mpidr)
			continue;
		if (mbox->pending != 0)
			return (-1);
		mbox->data.xc_func = func;
		mbox->data.xc_a1 = a1;
		mbox->data.xc_a2 = a2;
		mbox->data.xc_a3 = a3;
		mbox->done = 0;
		mbox->pending = 1;
	}
	dsb(ish);
	for (cpu = 0; cpu < platform->cpu_count; cpu++) {
		if (platform->cpu_mpidr[cpu] != boot_mpidr)
			gicv3_send_ipi_mpidr(platform->cpu_mpidr[cpu],
			    ARM64_XCALL_SGI);
	}

	deadline = arch_timer_gethrtime() + 1000000000ULL;
	for (cpu = 0; cpu < platform->cpu_count; cpu++) {
		struct arm64_xcall_mailbox *mbox = &arm64_xcall_mbox[cpu];

		if (platform->cpu_mpidr[cpu] == boot_mpidr)
			continue;
		while (mbox->done == 0 && arch_timer_gethrtime() < deadline)
			__asm__ __volatile__("wfe");
		if (mbox->done == 0)
			return (-1);
		if (results != 0)
			results[cpu] = mbox->result;
	}
	return (0);
}

static int
arm64_xcall_test_func(xc_arg_t a1, xc_arg_t a2, xc_arg_t a3)
{
	uint64_t mpidr = read_sysreg(mpidr_el1);

	return ((int)(a1 + a2 + a3 + (mpidr & 0xff)));
}

int
arm64_smp_test_xcall(const struct arm64_platform *platform)
{
	uint64_t boot_mpidr = read_sysreg(mpidr_el1) & MPIDR_AFFINITY_MASK;
	int results[ARM64_FDT_MAX_CPUS] = { 0 };
	uint32_t cpu;

	if (arm64_xcall_sync(platform, arm64_xcall_test_func, 10, 20, 30,
	    results) != 0)
		return (-1);
	for (cpu = 0; cpu < platform->cpu_count; cpu++) {
		if (platform->cpu_mpidr[cpu] == boot_mpidr)
			continue;
		if (results[cpu] !=
		    60 + (int)(platform->cpu_mpidr[cpu] & 0xff))
			return (-1);
	}
	return (0);
}

static int
arm64_xcall_cpu_abi_test(xc_arg_t a1, xc_arg_t a2, xc_arg_t a3)
{
	(void) a1;
	(void) a2;
	(void) a3;
	return (arm64_boot_cpu_selftest_id(arm64_smp_current_cpuid()));
}

int
arm64_smp_test_cpu_abi(const struct arm64_platform *platform)
{
	uint64_t boot_mpidr = read_sysreg(mpidr_el1) & MPIDR_AFFINITY_MASK;
	int results[ARM64_FDT_MAX_CPUS] = { 0 };
	uint32_t cpu;

	if (arm64_boot_cpu_selftest_id(arm64_smp_current_cpuid()) != 0)
		return (-1);
	if (arm64_xcall_sync(platform, arm64_xcall_cpu_abi_test, 0, 0, 0,
	    results) != 0)
		return (-1);
	for (cpu = 0; cpu < platform->cpu_count; cpu++) {
		if (platform->cpu_mpidr[cpu] != boot_mpidr && results[cpu] != 0)
			return (-1);
	}
	return (0);
}

/*
 * Exercise the SGI path that the common kernel's cross-call layer will use.
 */
int
arm64_smp_test_ipi(const struct arm64_platform *platform)
{
	uint64_t boot_mpidr = read_sysreg(mpidr_el1) & MPIDR_AFFINITY_MASK;
	uint64_t deadline, expected = 0;
	uint32_t cpu;

	arm64_ipi_count = 0;
	dsb(sy);
	for (cpu = 0; cpu < platform->cpu_count; cpu++) {
		if (platform->cpu_mpidr[cpu] == boot_mpidr)
			continue;
		gicv3_send_ipi_mpidr(platform->cpu_mpidr[cpu],
		    ARM64_XCALL_SGI);
		expected++;
	}
	deadline = arch_timer_gethrtime() + 1000000000ULL;
	while (arm64_ipi_count < expected &&
	    arch_timer_gethrtime() < deadline)
		SMT_PAUSE();
	return (arm64_ipi_count == expected ? 0 : -1);
}

void
mp_startup(uint64_t cpuid)
{
	if (arm64_boot_cpu_init_secondary((uint32_t)cpuid) != 0) {
		pl011_puts("bootstrap: secondary CPU state initialization failed\n");
		for (;;)
			__asm__ __volatile__("wfi");
	}
	gicv3_cpu_init();
	pl011_puts("bootstrap: secondary CPU online\n");
	(void) arm64_atomic_add_64(&arm64_online_cpus, 1);
	dsb(sy);
	__asm__ __volatile__("sev");
	arm64_intr_enable();
	for (;;)
		__asm__ __volatile__("wfi");
}
