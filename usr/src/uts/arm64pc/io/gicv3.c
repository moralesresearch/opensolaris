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
 * ARM GICv3 (Generic Interrupt Controller v3) driver.
 * This replaces the APIC/x2APIC driver on x86.
 *
 * The GICv3 consists of:
 * - Distributor (GICD): global, manages SPIs (shared interrupts)
 * - Redistributor (GICR): per-CPU, manages SGIs/PPIs
 * - CPU Interface (ICC): per-CPU, system register based
 */

#include <sys/types.h>
#include <sys/controlregs.h>
#include <sys/gicv3.h>
#include <sys/platform.h>

/* Firmware-discovered bases, with QEMU values as pre-FDT fallbacks. */
static uint64_t gicd_base = ARM64PC_GICD_BASE;
static uint64_t gicr_base = ARM64PC_GICR_BASE;
#define	GICD_BASE	gicd_base
#define	GICR_BASE	gicr_base
#define	GICR_STRIDE	0x20000ULL	/* Per-CPU redistributor size */

/* Register access helpers */
static inline uint32_t
gicd_read(uint64_t offset)
{
	return (*(volatile uint32_t *)(GICD_BASE + offset));
}

static inline void
gicd_write(uint64_t offset, uint32_t val)
{
	*(volatile uint32_t *)(GICD_BASE + offset) = val;
}

static inline uint32_t
gicr_read(int cpu, uint64_t offset)
{
	return (*(volatile uint32_t *)(GICR_BASE + cpu * GICR_STRIDE + offset));
}

static inline void
gicr_write(int cpu, uint64_t offset, uint32_t val)
{
	*(volatile uint32_t *)(GICR_BASE + cpu * GICR_STRIDE + offset) = val;
}

/* GICD register offsets */
#define	GICD_CTLR_OFF		0x0000
#define	GICD_TYPER_OFF		0x0004
#define	GICD_IGROUPR_OFF(n)	(0x0080 + 4 * (n))
#define	GICD_ISENABLER_OFF(n)	(0x0100 + 4 * (n))
#define	GICD_ICENABLER_OFF(n)	(0x0180 + 4 * (n))
#define	GICD_IPRIORITYR_OFF(n)	(0x0400 + 4 * (n))
#define	GICD_IROUTER_OFF(n)	(0x6000 + 8 * (n))
#define	GICD_ICFGR_OFF(n)	(0x0C00 + 4 * (n))

/* GICR SGI/PPI frame offsets */
#define	GICR_SGI_OFF		0x10000
#define	GICR_IGROUPR0_OFF	(GICR_SGI_OFF + 0x0080)
#define	GICR_ISENABLER0_OFF	(GICR_SGI_OFF + 0x0100)
#define	GICR_ICENABLER0_OFF	(GICR_SGI_OFF + 0x0180)
#define	GICR_IPRIORITYR0_OFF(n)	(GICR_SGI_OFF + 0x0400 + 4 * (n))
#define	GICR_WAKER_OFF		0x0014

/* GICD_CTLR bits */
#define	GICD_CTLR_ENABLE_GRP1_NS	(1U << 1)
#define	GICD_CTLR_ARE_NS		(1U << 5)
#define	GICD_CTLR_RWP			(1U << 31)

/* ICC system register access */
#define	ICC_SRE_SRE	(1ULL << 0)

static uint32_t gic_nr_irqs;

/*
 * Redistributor index of the calling PE.  Redistributor frames on QEMU virt
 * are laid out in Aff0 order, so the low MPIDR affinity level is the index
 * into the GICR stride.  SGI/PPI state is per-redistributor, so every access
 * below INTID 32 must use the caller's own frame rather than frame 0.
 */
static int
gicv3_cpu_index(void)
{
	return ((int)(read_sysreg(mpidr_el1) & 0xff));
}

void
gicv3_set_base(uint64_t distributor, uint64_t redistributor)
{
	gicd_base = distributor;
	gicr_base = redistributor;
}

/*
 * Wait for distributor writes to complete.
 */
static void
gicd_wait_rwp(void)
{
	while (gicd_read(GICD_CTLR_OFF) & GICD_CTLR_RWP)
		;
}

/*
 * Initialize the GICv3 distributor (once, on BSP).
 */
void
gicv3_init(void)
{
	uint32_t typer, i;

	/* Read number of supported interrupts */
	typer = gicd_read(GICD_TYPER_OFF);
	gic_nr_irqs = ((typer & 0x1f) + 1) * 32;
	if (gic_nr_irqs > 1020)
		gic_nr_irqs = 1020;

	/* Disable distributor */
	gicd_write(GICD_CTLR_OFF, 0);
	gicd_wait_rwp();

	/* Disable all SPIs */
	for (i = 1; i < gic_nr_irqs / 32; i++) {
		gicd_write(GICD_ICENABLER_OFF(i), 0xffffffffU);
		gicd_write(GICD_IGROUPR_OFF(i), 0xffffffffU);
	}

	/* Set all SPI priorities to default (0xa0) */
	for (i = 8; i < gic_nr_irqs / 4; i++)
		gicd_write(GICD_IPRIORITYR_OFF(i), 0xa0a0a0a0U);

	/* Set all SPIs as level-triggered, Group 1 NS */
	for (i = 2; i < gic_nr_irqs / 16; i++)
		gicd_write(GICD_ICFGR_OFF(i), 0);

	/* Enable distributor with ARE (Affinity Routing Enable) */
	gicd_write(GICD_CTLR_OFF,
	    GICD_CTLR_ENABLE_GRP1_NS | GICD_CTLR_ARE_NS);
	gicd_wait_rwp();
}

/*
 * Initialize the GICv3 CPU interface (per-CPU).
 * Called on each CPU during startup.
 */
void
gicv3_cpu_init(void)
{
	uint64_t sre;
	int cpu = gicv3_cpu_index();
	uint32_t waker;
	int i;

	/* Enable system register access (ICC_SRE_EL1) */
	__asm__ __volatile__("mrs %0, S3_0_C12_C12_5" : "=r"(sre));
	sre |= ICC_SRE_SRE;
	__asm__ __volatile__("msr S3_0_C12_C12_5, %0" :: "r"(sre));
	isb();

	/* Wake up the redistributor */
	waker = gicr_read(cpu, GICR_WAKER_OFF);
	waker &= ~(1U << 1);	/* Clear ProcessorSleep */
	gicr_write(cpu, GICR_WAKER_OFF, waker);

	/* Wait for ChildrenAsleep to clear */
	while (gicr_read(cpu, GICR_WAKER_OFF) & (1U << 2))
		;

	/* Set priority mask to allow all priorities */
	__asm__ __volatile__("msr S3_0_C4_C6_0, %0" :: "r"(0xffULL));

	/* Enable Group 1 interrupts */
	__asm__ __volatile__("msr S3_0_C12_C12_7, %0" :: "r"(1ULL));

	/* Set all SGI/PPI priorities */
	for (i = 0; i < 8; i++)
		gicr_write(cpu, GICR_IPRIORITYR0_OFF(i), 0xa0a0a0a0U);
	/* SGI 1 is the high-PIL cross-call, PPI 30 is the clock. */
	gicr_write(cpu, GICR_IPRIORITYR0_OFF(0), 0xa0a020a0U);
	gicr_write(cpu, GICR_IPRIORITYR0_OFF(7), 0xa010a0a0U);

	/* Deliver SGIs and PPIs through the non-secure Group 1 interface. */
	gicr_write(cpu, GICR_IGROUPR0_OFF, 0xffffffffU);

	/* Enable all SGIs (0-15) */
	gicr_write(cpu, GICR_ISENABLER0_OFF, 0xffffU);

	isb();
}

/*
 * Acknowledge an interrupt — read IAR1.
 */
uint32_t
gicv3_irq_acknowledge(void)
{
	uint64_t irq;

	__asm__ __volatile__("mrs %0, S3_0_C12_C12_0" : "=r"(irq));
	return ((uint32_t)irq);
}

/*
 * Signal End-Of-Interrupt — write EOIR1.
 */
void
gicv3_irq_complete(uint32_t irq)
{
	__asm__ __volatile__("msr S3_0_C12_C12_1, %0" :: "r"((uint64_t)irq));
	isb();
}

/*
 * Enable a specific SPI interrupt.
 */
void
gicv3_irq_enable(uint32_t irq)
{
	uint32_t reg = irq / 32;
	uint32_t bit = irq % 32;

	if (irq < 32) {
		/* SGI/PPI: use this CPU's redistributor */
		gicr_write(gicv3_cpu_index(), GICR_ISENABLER0_OFF, 1U << bit);
	} else {
		/* SPI: use distributor */
		gicd_write(GICD_ISENABLER_OFF(reg), 1U << bit);
	}
}

/*
 * Disable a specific interrupt.
 */
void
gicv3_irq_disable(uint32_t irq)
{
	uint32_t reg = irq / 32;
	uint32_t bit = irq % 32;

	if (irq < 32) {
		/* SGI/PPI: use this CPU's redistributor */
		gicr_write(gicv3_cpu_index(), GICR_ICENABLER0_OFF, 1U << bit);
	} else {
		gicd_write(GICD_ICENABLER_OFF(reg), 1U << bit);
	}
}

/*
 * Set interrupt priority.
 */
void
gicv3_irq_set_priority(uint32_t irq, uint8_t priority)
{
	uint32_t reg = irq / 4;
	uint32_t shift = (irq % 4) * 8;
	uint32_t val;

	if (irq < 32) {
		/* SGI/PPI: use this CPU's redistributor */
		int cpu = gicv3_cpu_index();

		val = gicr_read(cpu, GICR_IPRIORITYR0_OFF(reg));
		val &= ~(0xffU << shift);
		val |= ((uint32_t)priority << shift);
		gicr_write(cpu, GICR_IPRIORITYR0_OFF(reg), val);
	} else {
		val = gicd_read(GICD_IPRIORITYR_OFF(reg));
		val &= ~(0xffU << shift);
		val |= ((uint32_t)priority << shift);
		gicd_write(GICD_IPRIORITYR_OFF(reg), val);
	}
}

/*
 * Set SPI routing (target CPU by MPIDR).
 */
void
gicv3_irq_set_target(uint32_t irq, uint64_t mpidr)
{
	if (irq >= 32) {
		/* IROUTER is 64-bit */
		volatile uint64_t *router = (volatile uint64_t *)
		    (GICD_BASE + GICD_IROUTER_OFF(irq));
		*router = mpidr & 0xff00ffffffULL;
	}
}

/*
 * Send an SGI (Software Generated Interrupt) — for IPIs.
 */
void
gicv3_send_ipi(int target_cpu, int vector)
{
	gicv3_send_ipi_mpidr((uint64_t)target_cpu, vector);
}

/*
 * Send an SGI to a PE identified by its MPIDR affinity.  A SGI target list
 * selects Aff0, while the remaining affinity levels are encoded separately
 * in ICC_SGI1R_EL1.
 */
void
gicv3_send_ipi_mpidr(uint64_t mpidr, int vector)
{
	uint64_t aff0 = mpidr & 0xff;
	uint64_t val;

	/*
	 * ICC_SGI1R_EL1 format:
	 *   TargetList [15:0], Aff1 [23:16], INTID [27:24],
	 *   Aff2 [39:32], IRM [40], Aff3 [55:48]
	 *
	 * A TargetList is only 16 bits wide.  Systems whose Aff0 CPU number is
	 * outside that range need an affinity-routing policy above this primitive.
	 */
	if (aff0 >= 16)
		return;
	val = (1ULL << aff0) |
	    (((mpidr >> 8) & 0xff) << 16) |
	    ((uint64_t)(vector & 0xf) << 24) |
	    (((mpidr >> 16) & 0xff) << 32) |
	    (((mpidr >> 32) & 0xff) << 48);
	dsb(ishst);
	__asm__ __volatile__("msr S3_0_C12_C11_5, %0" :: "r"(val));
	isb();
}
