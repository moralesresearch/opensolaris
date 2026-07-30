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
 * ARM GICv3 (Generic Interrupt Controller v3) definitions.
 * Replaces APIC on x86.
 */

#ifndef _SYS_GICV3_H
#define	_SYS_GICV3_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * GIC interrupt types.
 */
#define	GIC_SPI_BASE		32	/* Shared Peripheral Interrupts start */
#define	GIC_PPI_BASE		16	/* Private Peripheral Interrupts start */
#define	GIC_SGI_BASE		0	/* Software Generated Interrupts start */

#define	GIC_MAX_IRQS		1020	/* Maximum interrupt IDs */
#define	GIC_SPURIOUS_IRQ	1023	/* Spurious interrupt */

/*
 * GIC Distributor (GICD) register offsets.
 */
#define	GICD_CTLR		0x0000	/* Distributor Control */
#define	GICD_TYPER		0x0004	/* Interrupt Controller Type */
#define	GICD_IIDR		0x0008	/* Implementer Identification */
#define	GICD_STATUSR		0x0010	/* Error Reporting Status */
#define	GICD_IGROUPR(n)		(0x0080 + 4 * (n))	/* Interrupt Group */
#define	GICD_ISENABLER(n)	(0x0100 + 4 * (n))	/* Set-Enable */
#define	GICD_ICENABLER(n)	(0x0180 + 4 * (n))	/* Clear-Enable */
#define	GICD_ISPENDR(n)		(0x0200 + 4 * (n))	/* Set-Pending */
#define	GICD_ICPENDR(n)		(0x0280 + 4 * (n))	/* Clear-Pending */
#define	GICD_ISACTIVER(n)	(0x0300 + 4 * (n))	/* Set-Active */
#define	GICD_ICACTIVER(n)	(0x0380 + 4 * (n))	/* Clear-Active */
#define	GICD_IPRIORITYR(n)	(0x0400 + 4 * (n))	/* Priority */
#define	GICD_ITARGETSR(n)	(0x0800 + 4 * (n))	/* Target (GICv2) */
#define	GICD_ICFGR(n)		(0x0C00 + 4 * (n))	/* Configuration */
#define	GICD_IROUTER(n)		(0x6000 + 8 * (n))	/* Routing (GICv3) */

/* GICD_CTLR bits */
#define	GICD_CTLR_ENABLE_GRP0	(1U << 0)
#define	GICD_CTLR_ENABLE_GRP1_NS (1U << 1)
#define	GICD_CTLR_ENABLE_GRP1_S	(1U << 2)
#define	GICD_CTLR_ARE_S		(1U << 4)
#define	GICD_CTLR_ARE_NS	(1U << 5)
#define	GICD_CTLR_DS		(1U << 6)
#define	GICD_CTLR_RWP		(1U << 31)

/* GICD_TYPER fields */
#define	GICD_TYPER_ITLINES_MASK	0x1f
#define	GICD_TYPER_CPUNUM_SHIFT	5
#define	GICD_TYPER_CPUNUM_MASK	(0x7 << GICD_TYPER_CPUNUM_SHIFT)

/*
 * GIC Redistributor (GICR) register offsets.
 * Each CPU has its own redistributor frame.
 */
#define	GICR_CTLR		0x0000
#define	GICR_IIDR		0x0004
#define	GICR_TYPER		0x0008	/* 64-bit */
#define	GICR_STATUSR		0x0010
#define	GICR_WAKER		0x0014
#define	GICR_PROPBASER		0x0070	/* LPI config table (64-bit) */
#define	GICR_PENDBASER		0x0078	/* LPI pending table (64-bit) */

/* GICR_WAKER bits */
#define	GICR_WAKER_PS		(1U << 1)	/* Processor Sleep */
#define	GICR_WAKER_CA		(1U << 2)	/* Children Asleep */

/* SGI/PPI redistributor frame (offset 0x10000 from GICR base) */
#define	GICR_SGI_BASE		0x10000
#define	GICR_IGROUPR0		(GICR_SGI_BASE + 0x0080)
#define	GICR_ISENABLER0		(GICR_SGI_BASE + 0x0100)
#define	GICR_ICENABLER0		(GICR_SGI_BASE + 0x0180)
#define	GICR_ISPENDR0		(GICR_SGI_BASE + 0x0200)
#define	GICR_ICPENDR0		(GICR_SGI_BASE + 0x0280)
#define	GICR_IPRIORITYR(n)	(GICR_SGI_BASE + 0x0400 + 4 * (n))
#define	GICR_ICFGR(n)		(GICR_SGI_BASE + 0x0C00 + 4 * (n))

/* GICR_TYPER bits */
#define	GICR_TYPER_LAST		(1UL << 4)	/* Last redistributor */
#define	GICR_TYPER_AFFVAL_SHIFT	32

/*
 * GICv3 CPU Interface — accessed via system registers (ICC_*).
 */
#define	ICC_SRE_EL1_SRE		(1UL << 0)	/* System Register Enable */
#define	ICC_SRE_EL1_DFB		(1UL << 1)	/* Disable FIQ Bypass */
#define	ICC_SRE_EL1_DIB		(1UL << 2)	/* Disable IRQ Bypass */

/* ICC system register accessors */
#define	read_icc_sre_el1()	read_sysreg(S3_0_C12_C12_5)
#define	write_icc_sre_el1(v)	write_sysreg(S3_0_C12_C12_5, v)
#define	read_icc_iar1_el1()	read_sysreg(S3_0_C12_C12_0)
#define	write_icc_eoir1_el1(v)	write_sysreg(S3_0_C12_C12_1, v)
#define	write_icc_pmr_el1(v)	write_sysreg(S3_0_C4_C6_0, v)
#define	read_icc_pmr_el1()	read_sysreg(S3_0_C4_C6_0)
#define	write_icc_igrpen1_el1(v) write_sysreg(S3_0_C12_C12_7, v)
#define	write_icc_sgi1r_el1(v)	write_sysreg(S3_0_C12_C11_5, v)
#define	read_icc_ctlr_el1()	read_sysreg(S3_0_C12_C12_4)
#define	write_icc_ctlr_el1(v)	write_sysreg(S3_0_C12_C12_4, v)

#if !defined(_ASM)

#include <sys/types.h>
#include <sys/controlregs.h>

/*
 * GICv3 software state.
 */
struct gicv3_softc {
	volatile uint8_t	*gicd_base;	/* Distributor MMIO base */
	volatile uint8_t	*gicr_base;	/* Redistributor MMIO base */
	uint32_t		gicd_typer;	/* Cached GICD_TYPER */
	uint32_t		nr_irqs;	/* Number of supported IRQs */
};

extern void gicv3_init(void);
extern void gicv3_set_base(uint64_t, uint64_t);
extern void gicv3_cpu_init(void);
extern void gicv3_send_ipi(int target_cpu, int vector);
extern void gicv3_send_ipi_mpidr(uint64_t, int);
extern uint32_t gicv3_irq_acknowledge(void);
extern void gicv3_irq_complete(uint32_t irq);
extern void gicv3_irq_enable(uint32_t irq);
extern void gicv3_irq_disable(uint32_t irq);
extern void gicv3_irq_set_priority(uint32_t irq, uint8_t priority);
extern void gicv3_irq_set_target(uint32_t irq, uint64_t mpidr);

#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_GICV3_H */
