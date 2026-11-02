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
 * Multiprocessor start-up and cross-CPU tests for the amd64pc bootstrap.
 *
 * Application processors listed in the ACPI MADT are started one at a
 * time with the INIT-SIPI-SIPI sequence through ml/ap_trampoline.s, load
 * their own GDT/TSS, enable their local APIC and park in an interrupt
 * enabled idle loop.  The spin lock is the machlock.h lock_t ABI
 * (lock_set/lock_try/lock_clear, LOCK_HELD_VALUE) implemented with
 * compiler atomics in place of uts/intel/ia32/ml/lock_prim.s.
 */

#include <sys/types.h>
#include <sys/machlock.h>
#include <sys/apic.h>
#include <sys/controlregs.h>
#include <sys/amd64pc.h>

#define	AP_STACK_SIZE	16384
#define	LOCK_ITERS	20000

struct amd64pc_cpu {
	struct amd64pc_cpu *cpu_self;
	uint32_t	cpu_id;
	uint32_t	cpu_apicid;
	volatile uint32_t cpu_online;
	volatile uint32_t cpu_ipis;
};

extern char ap_tramp_start[], ap_tramp_end[];
extern char ap_tramp_cr3[], ap_tramp_efer[], ap_tramp_stack[];
extern char ap_tramp_entry[], ap_tramp_cpu[];

static struct amd64pc_cpu cpus[AMD64PC_MAX_CPUS];
static uint8_t ap_stacks[AMD64PC_MAX_CPUS][AP_STACK_SIZE]
    __attribute__((aligned(16)));
static volatile uint32_t smp_online_count = 1;
static volatile uint32_t lock_go, lock_done;
static lock_t test_lock;
static volatile uint64_t lock_counter;

void
lock_set(lock_t *lp)
{
	while (!lock_try(lp)) {
		while (__atomic_load_n(lp, __ATOMIC_RELAXED) != 0)
			__asm__ __volatile__("pause");
	}
}

int
lock_try(lock_t *lp)
{
	return (__atomic_exchange_n(lp, (lock_t)LOCK_HELD_VALUE,
	    __ATOMIC_ACQUIRE) == 0);
}

void
lock_clear(lock_t *lp)
{
	__atomic_store_n(lp, 0, __ATOMIC_RELEASE);
}

uint32_t
amd64pc_cpuid(void)
{
	struct amd64pc_cpu *cp = (struct amd64pc_cpu *)(uintptr_t)
	    pc_rdmsr(MSR_AMD_GSBASE);

	return (cp == 0 ? 0 : cp->cpu_id);
}

uint32_t
smp_ncpus_online(void)
{
	return (smp_online_count);
}

static void
cpu_publish(uint32_t id)
{
	cpus[id].cpu_self = &cpus[id];
	cpus[id].cpu_id = id;
	pc_wrmsr(MSR_AMD_GSBASE, (uint64_t)(uintptr_t)&cpus[id]);
}

static void
ipi_intr(struct regs *rp)
{
	(void) rp;
	__atomic_add_fetch(&cpus[amd64pc_cpuid()].cpu_ipis, 1,
	    __ATOMIC_RELAXED);
}

static void
lock_worker(void)
{
	int i;

	for (i = 0; i < LOCK_ITERS; i++) {
		lock_set(&test_lock);
		lock_counter++;
		lock_clear(&test_lock);
	}
	__atomic_add_fetch(&lock_done, 1, __ATOMIC_RELEASE);
}

/* C entry from the trampoline: on the AP's own stack, long mode, IF=0. */
void __attribute__((noreturn))
amd64pc_ap_main(uint32_t id)
{
	desctbl_init(id, (uint64_t)(uintptr_t)&ap_stacks[id][AP_STACK_SIZE]);
	desctbl_load(id);
	cpu_publish(id);
	lapic_init();
	cpus[id].cpu_apicid = lapic_id();
	__atomic_store_n(&cpus[id].cpu_online, 1, __ATOMIC_RELEASE);
	__atomic_add_fetch(&smp_online_count, 1, __ATOMIC_RELEASE);

	/* Take IPIs while waiting for the lock test to begin. */
	__asm__ __volatile__("sti");
	while (__atomic_load_n(&lock_go, __ATOMIC_ACQUIRE) == 0)
		__asm__ __volatile__("pause");
	__asm__ __volatile__("cli");
	lock_worker();

	for (;;)
		__asm__ __volatile__("sti; hlt; cli");
}

static int
tramp_page_usable(void)
{
	const struct amd64pc_bootinfo *bi = &amd64pc_bootinfo;
	uint32_t m;

	for (m = 0; m < bi->bi_nmem; m++)
		if (bi->bi_mem[m].ms_base <= AMD64PC_AP_TRAMPOLINE &&
		    bi->bi_mem[m].ms_base + bi->bi_mem[m].ms_size >=
		    AMD64PC_AP_TRAMPOLINE + 4096)
			return (1);
	return (0);
}

#define	TRAMP_SLOT(sym)	((void *)(uintptr_t)(AMD64PC_AP_TRAMPOLINE + \
	((sym) - ap_tramp_start)))

static int
start_ap(uint32_t id, uint32_t apicid)
{
	uint64_t deadline;

	*(volatile uint64_t *)TRAMP_SLOT(ap_tramp_stack) =
	    (uint64_t)(uintptr_t)&ap_stacks[id][AP_STACK_SIZE];
	*(volatile uint32_t *)TRAMP_SLOT(ap_tramp_cpu) = id;
	__atomic_thread_fence(__ATOMIC_SEQ_CST);

	/* INIT assert, then two STARTUP IPIs (Intel SDM 8.4.4.1). */
	lapic_send_ipi(apicid, AV_RESET | AV_ASSERT);
	tsc_delay_us(10000);
	lapic_send_ipi(apicid, AV_STARTUP | (AMD64PC_AP_TRAMPOLINE >> 12));
	tsc_delay_us(200);
	if (!cpus[id].cpu_online)
		lapic_send_ipi(apicid,
		    AV_STARTUP | (AMD64PC_AP_TRAMPOLINE >> 12));

	deadline = pc_rdtsc() + tsc_hz / 5;
	while (!__atomic_load_n(&cpus[id].cpu_online, __ATOMIC_ACQUIRE)) {
		if (pc_rdtsc() > deadline)
			return (-1);
		__asm__ __volatile__("pause");
	}
	return (0);
}

int
smp_start(void)
{
	const struct amd64pc_platform *pl = &amd64pc_platform;
	uint32_t bsp = lapic_id(), i, id = 1;
	uint64_t efer = pc_rdmsr(MSR_AMD_EFER);
	char *dst = (char *)(uintptr_t)AMD64PC_AP_TRAMPOLINE;
	const char *src;

	cpu_publish(0);
	cpus[0].cpu_apicid = bsp;
	cpus[0].cpu_online = 1;
	trap_set_handler(AMD64PC_VEC_IPI, ipi_intr);

	if (pl->cpu_count <= 1)
		return (0);
	if (!tramp_page_usable()) {
		cons_puts("amd64pc: AP trampoline page is not free RAM; "
		    "running uniprocessor\n");
		return (0);
	}
	for (src = ap_tramp_start; src < ap_tramp_end; src++)
		*dst++ = *src;
	*(volatile uint64_t *)TRAMP_SLOT(ap_tramp_cr3) = pc_getcr3();
	*(volatile uint64_t *)TRAMP_SLOT(ap_tramp_efer) = efer;
	*(volatile uint64_t *)TRAMP_SLOT(ap_tramp_entry) =
	    (uint64_t)(uintptr_t)amd64pc_ap_main;

	for (i = 0; i < pl->cpu_count; i++) {
		if (pl->cpu_apicid[i] == bsp)
			continue;
		cpus[id].cpu_apicid = pl->cpu_apicid[i];
		if (start_ap(id, pl->cpu_apicid[i]) != 0) {
			cons_puts("amd64pc: CPU with APIC id ");
			cons_putdec(pl->cpu_apicid[i]);
			cons_puts(" did not start\n");
			continue;
		}
		id++;
	}
	return (0);
}

/*
 * Cross-CPU checks once the APs are parked: a fixed-vector IPI to each
 * AP, then every CPU hammering one lock_t-protected counter.
 */
int
smp_selftest(void)
{
	uint32_t n = smp_online_count, i;
	uint64_t deadline;

	for (i = 1; i < n; i++)
		lapic_send_ipi(cpus[i].cpu_apicid, AV_FIXED | AMD64PC_VEC_IPI);
	deadline = pc_rdtsc() + tsc_hz;
	for (i = 1; i < n; i++) {
		while (__atomic_load_n(&cpus[i].cpu_ipis, __ATOMIC_ACQUIRE) == 0) {
			if (pc_rdtsc() > deadline)
				return (-1);
			__asm__ __volatile__("pause");
		}
	}
	if (n > 1)
		cons_puts("amd64pc: interprocessor interrupt IPI_OK\n");

	lock_counter = 0;
	__atomic_store_n(&lock_go, 1, __ATOMIC_RELEASE);
	lock_worker();
	deadline = pc_rdtsc() + 5 * tsc_hz;
	while (__atomic_load_n(&lock_done, __ATOMIC_ACQUIRE) != n) {
		if (pc_rdtsc() > deadline)
			return (-2);
		__asm__ __volatile__("pause");
	}
	if (lock_counter != (uint64_t)n * LOCK_ITERS)
		return (-3);
	cons_puts("amd64pc: cross-CPU lock_t counter ");
	cons_putdec(lock_counter);
	cons_puts(" LOCK_OK\n");
	return (0);
}
