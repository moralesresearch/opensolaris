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
 * Local APIC (xAPIC MMIO or x2APIC MSR mode), legacy 8259 shutdown and
 * PIT-calibrated TSC time base for the amd64pc bootstrap.  Register
 * indices and ICR/LVT bits are the real ones from uts/i86pc/sys/apic.h,
 * which expresses them as 32-bit word indices into the xAPIC page.
 */

#include <sys/types.h>
#include <sys/apic.h>
#include <sys/x86_archext.h>
#include <sys/amd64pc.h>

#define	APIC_BASE_ENABLE	(1ULL << 11)
#define	APIC_BASE_X2APIC	(1ULL << 10)
#define	X2APIC_MSR_BASE		0x800
#define	X2APIC_MSR_ICR		0x830

#define	APIC_SVR_ENABLE		0x100
#define	APIC_TIMER_DIV16	0x3
#define	APIC_ICR_BUSY		0x1000		/* AV_PENDING */

#define	PIT_HZ			1193182ULL
#define	PIT_CH2			0x42
#define	PIT_MODE		0x43
#define	PIT_GATE		0x61

uint64_t tsc_hz;
static volatile uint32_t *lapic_va;
static int x2apic_mode;
static volatile uint64_t timer_ticks;

static uint32_t
lapic_read(uint32_t idx)
{
	if (x2apic_mode)
		return ((uint32_t)pc_rdmsr(X2APIC_MSR_BASE + (idx >> 2)));
	return (lapic_va[idx]);
}

static void
lapic_write(uint32_t idx, uint32_t v)
{
	if (x2apic_mode)
		pc_wrmsr(X2APIC_MSR_BASE + (idx >> 2), v);
	else
		lapic_va[idx] = v;
}

uint32_t
lapic_id(void)
{
	uint32_t id = lapic_read(APIC_LID_REG);

	return (x2apic_mode ? id : id >> APIC_ICR_ID_BIT_OFFSET);
}

void
lapic_eoi(void)
{
	lapic_write(APIC_EOI_REG, 0);
}

/* Move the 8259s off the exception vectors, then mask them for good. */
static void
pic_disable(void)
{
	pc_outb(0x20, 0x11);
	pc_outb(0xa0, 0x11);
	pc_outb(0x21, 0x20);
	pc_outb(0xa1, 0x28);
	pc_outb(0x21, 0x04);
	pc_outb(0xa1, 0x02);
	pc_outb(0x21, 0x01);
	pc_outb(0xa1, 0x01);
	pc_outb(0x21, 0xff);
	pc_outb(0xa1, 0xff);
}

static void
lapic_spurious(struct regs *rp)
{
	(void) rp;
}

void
lapic_init(void)
{
	uint64_t base = pc_rdmsr(REG_APIC_BASE_MSR);
	static int bsp_done;

	if (!bsp_done) {
		pic_disable();
		trap_set_handler(AMD64PC_VEC_SPURIOUS, lapic_spurious);
		if (amd64pc_platform.lapic_base == 0)
			amd64pc_platform.lapic_base = base & ~0xfffULL;
		bsp_done = 1;
	}
	x2apic_mode = (base & APIC_BASE_X2APIC) != 0;
	if ((base & APIC_BASE_ENABLE) == 0)
		pc_wrmsr(REG_APIC_BASE_MSR, base | APIC_BASE_ENABLE);
	lapic_va = (volatile uint32_t *)(uintptr_t)amd64pc_platform.lapic_base;

	lapic_write(APIC_TASK_REG, 0);
	lapic_write(APIC_LOCAL_TIMER, AV_MASK);
	lapic_write(APIC_INT_VECT0, AV_MASK);		/* LINT0: no 8259 */
	lapic_write(APIC_ERR_VECT, AV_MASK);
	lapic_write(APIC_SPUR_INT_REG, APIC_SVR_ENABLE | AMD64PC_VEC_SPURIOUS);
	lapic_eoi();
}

void
lapic_send_ipi(uint32_t apicid, uint32_t icr_low)
{
	if (x2apic_mode) {
		pc_wrmsr(X2APIC_MSR_ICR, ((uint64_t)apicid << 32) | icr_low);
		return;
	}
	(void) lapic_ipi_wait();
	lapic_write(APIC_INT_CMD2, apicid << APIC_ICR_ID_BIT_OFFSET);
	lapic_write(APIC_INT_CMD1, icr_low);
}

int
lapic_ipi_wait(void)
{
	int spin;

	if (x2apic_mode)
		return (0);
	for (spin = 0; spin < 1000000; spin++) {
		if ((lapic_read(APIC_INT_CMD1) & APIC_ICR_BUSY) == 0)
			return (0);
		__asm__ __volatile__("pause");
	}
	return (-1);
}

/*
 * Calibrate the TSC against 10 ms of PIT channel 2 (mode 0, gate on,
 * speaker off) and fall back to CPUID leaf 0x16 or a 1 GHz guess when
 * the platform has no PIT.
 */
void
tsc_calibrate(void)
{
	uint32_t count = (uint32_t)(PIT_HZ / 100);
	uint64_t t0, t1, spin;
	uint32_t a, b, c, d;

	pc_outb(PIT_GATE, (pc_inb(PIT_GATE) & ~0x02) | 0x01);
	pc_outb(PIT_MODE, 0xb0);
	pc_outb(PIT_CH2, count & 0xff);
	pc_outb(PIT_CH2, count >> 8);
	t0 = pc_rdtsc();
	for (spin = 0; spin < 100000000ULL; spin++)
		if ((pc_inb(PIT_GATE) & 0x20) != 0)
			break;
	t1 = pc_rdtsc();
	if (spin < 100000000ULL && t1 > t0) {
		tsc_hz = (t1 - t0) * 100;
		return;
	}
	pc_cpuid(0, 0, &a, &b, &c, &d);
	if (a >= 0x16) {
		pc_cpuid(0x16, 0, &a, &b, &c, &d);
		if (a != 0) {
			tsc_hz = (uint64_t)a * 1000000ULL;
			return;
		}
	}
	tsc_hz = 1000000000ULL;
}

uint64_t
amd64pc_gethrtime(void)
{
	uint64_t t = pc_rdtsc();

	/* Split so the product cannot overflow for any sane frequency. */
	return ((t / tsc_hz) * 1000000000ULL +
	    ((t % tsc_hz) * 1000000000ULL) / tsc_hz);
}

void
tsc_delay_us(uint64_t us)
{
	uint64_t end = pc_rdtsc() + us * (tsc_hz / 1000000ULL);

	while (pc_rdtsc() < end)
		__asm__ __volatile__("pause");
}

static void
lapic_timer_intr(struct regs *rp)
{
	(void) rp;
	__atomic_add_fetch(&timer_ticks, 1, __ATOMIC_RELAXED);
}

/*
 * Run the APIC timer periodically until five interrupts have been
 * taken, which proves IDT delivery, EOI and interrupt re-enable.
 * Returns the measured APIC timer input frequency in Hz, or 0.
 */
int
lapic_timer_test(void)
{
	const uint32_t initial = 200000;
	uint64_t start, deadline, ticks, elapsed;

	trap_set_handler(AMD64PC_VEC_TIMER, lapic_timer_intr);
	timer_ticks = 0;
	lapic_write(APIC_DIVIDE_REG, APIC_TIMER_DIV16);
	lapic_write(APIC_LOCAL_TIMER, AV_TIME | AMD64PC_VEC_TIMER);
	start = pc_rdtsc();
	lapic_write(APIC_INIT_COUNT, initial);
	deadline = start + 2 * tsc_hz;

	__asm__ __volatile__("sti");
	while (timer_ticks < 5 && pc_rdtsc() < deadline)
		__asm__ __volatile__("hlt");
	__asm__ __volatile__("cli");
	elapsed = pc_rdtsc() - start;
	ticks = timer_ticks;

	lapic_write(APIC_LOCAL_TIMER, AV_MASK);
	lapic_write(APIC_INIT_COUNT, 0);
	if (ticks < 5 || elapsed == 0)
		return (-1);
	cons_puts("amd64pc: APIC timer ");
	cons_putdec(ticks * initial * 16 * (tsc_hz / 1000) /
	    (elapsed / 1000 + 1));
	cons_puts(" Hz input, ");
	cons_putdec(ticks);
	cons_puts(" interrupts\n");
	return (0);
}
