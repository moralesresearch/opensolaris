/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2026 OpenSolaris AArch64 Contributors.
 * AArch64 system register definitions and accessors.
 */

#ifndef _SYS_CONTROLREGS_H
#define	_SYS_CONTROLREGS_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * System register read/write macros for C code.
 * ARM64 system registers are accessed via MRS (read) and MSR (write).
 */
#define	read_sysreg(reg)					\
({								\
	uint64_t __val;						\
	__asm__ __volatile__("mrs %0, " #reg : "=r" (__val));	\
	__val;							\
})

#define	write_sysreg(reg, val)					\
({								\
	uint64_t __val = (uint64_t)(val);			\
	__asm__ __volatile__("msr " #reg ", %0" :: "r" (__val));\
})

/*
 * Instruction/Data/Memory barrier macros
 */
#define	isb()	__asm__ __volatile__("isb" ::: "memory")
#define	dsb(t)	__asm__ __volatile__("dsb " #t ::: "memory")
#define	dmb(t)	__asm__ __volatile__("dmb " #t ::: "memory")

/*
 * SCTLR_EL1 — System Control Register (EL1)
 */
#define	SCTLR_M	(1UL << 0)	/* MMU enable */
#define	SCTLR_A	(1UL << 1)	/* Alignment check enable */
#define	SCTLR_C	(1UL << 2)	/* Data cache enable */
#define	SCTLR_SA	(1UL << 3)	/* SP alignment check EL1 */
#define	SCTLR_SA0	(1UL << 4)	/* SP alignment check EL0 */
#define	SCTLR_CP15BEN	(1UL << 5)	/* CP15 barrier enable (AArch32) */
#define	SCTLR_nAA	(1UL << 6)	/* Non-aligned access */
#define	SCTLR_ITD	(1UL << 7)	/* IT disable (AArch32) */
#define	SCTLR_SED	(1UL << 8)	/* SETEND disable (AArch32) */
#define	SCTLR_UMA	(1UL << 9)	/* User mask access */
#define	SCTLR_EnRCTX	(1UL << 10)	/* EL0 RCTX access */
#define	SCTLR_I	(1UL << 12)	/* Instruction cache enable */
#define	SCTLR_EnDB	(1UL << 13)	/* PAC: Data brk enable */
#define	SCTLR_DZE	(1UL << 14)	/* DC ZVA EL0 enable */
#define	SCTLR_UCT	(1UL << 15)	/* EL0 CTR_EL0 access */
#define	SCTLR_nTWI	(1UL << 16)	/* WFI not trap */
#define	SCTLR_nTWE	(1UL << 18)	/* WFE not trap */
#define	SCTLR_WXN	(1UL << 19)	/* Write implies XN */
#define	SCTLR_TSCXT	(1UL << 20)	/* Trap SCXTNUM access */
#define	SCTLR_IESB	(1UL << 21)	/* Implicit ESB */
#define	SCTLR_EIS	(1UL << 22)	/* Exception entry is context sync */
#define	SCTLR_SPAN	(1UL << 23)	/* Set privileged access never */
#define	SCTLR_E0E	(1UL << 24)	/* EL0 endianness (0=LE) */
#define	SCTLR_EE	(1UL << 25)	/* Exception endianness (0=LE) */
#define	SCTLR_UCI	(1UL << 26)	/* EL0 cache instructions */

/*
 * TCR_EL1 — Translation Control Register
 */
#define	TCR_T0SZ_SHIFT	0
#define	TCR_T0SZ_MASK	(0x3fUL << TCR_T0SZ_SHIFT)
#define	TCR_EPD0	(1UL << 7)	/* Disable TTBR0 walks */
#define	TCR_IRGN0_SHIFT	8
#define	TCR_ORGN0_SHIFT	10
#define	TCR_SH0_SHIFT	12
#define	TCR_TG0_SHIFT	14	/* TTBR0 granule: 00=4K, 01=64K, 10=16K */
#define	TCR_T1SZ_SHIFT	16
#define	TCR_T1SZ_MASK	(0x3fUL << TCR_T1SZ_SHIFT)
#define	TCR_A1		(1UL << 22)	/* ASID from TTBR1 */
#define	TCR_EPD1	(1UL << 23)	/* Disable TTBR1 walks */
#define	TCR_IRGN1_SHIFT	24
#define	TCR_ORGN1_SHIFT	26
#define	TCR_SH1_SHIFT	28
#define	TCR_TG1_SHIFT	30	/* TTBR1 granule: 01=16K, 10=4K, 11=64K */
#define	TCR_IPS_SHIFT	32	/* Intermediate phys addr size */
#define	TCR_AS_16BIT	(1UL << 36)	/* 16-bit ASID */
#define	TCR_TBI0	(1UL << 37)	/* Top byte ignored TTBR0 */
#define	TCR_TBI1	(1UL << 38)	/* Top byte ignored TTBR1 */

/* Cacheability / shareability attribute values */
#define	TCR_RGN_NC	0x0UL	/* Non-cacheable */
#define	TCR_RGN_WBWA	0x1UL	/* Write-back, write-allocate */
#define	TCR_RGN_WT	0x2UL	/* Write-through */
#define	TCR_RGN_WBnWA	0x3UL	/* Write-back, no write-allocate */
#define	TCR_SH_NS	0x0UL	/* Non-shareable */
#define	TCR_SH_OS	0x2UL	/* Outer shareable */
#define	TCR_SH_IS	0x3UL	/* Inner shareable */

/* Granule size values for TCR_TG0 */
#define	TCR_TG0_4K	(0x0UL << TCR_TG0_SHIFT)
#define	TCR_TG0_64K	(0x1UL << TCR_TG0_SHIFT)
#define	TCR_TG0_16K	(0x2UL << TCR_TG0_SHIFT)

/* Granule size values for TCR_TG1 */
#define	TCR_TG1_16K	(0x1UL << TCR_TG1_SHIFT)
#define	TCR_TG1_4K	(0x2UL << TCR_TG1_SHIFT)
#define	TCR_TG1_64K	(0x3UL << TCR_TG1_SHIFT)

/* IPS values (physical address size) */
#define	TCR_IPS_32BIT	(0x0UL << TCR_IPS_SHIFT)	/* 4GB */
#define	TCR_IPS_36BIT	(0x1UL << TCR_IPS_SHIFT)	/* 64GB */
#define	TCR_IPS_40BIT	(0x2UL << TCR_IPS_SHIFT)	/* 1TB */
#define	TCR_IPS_42BIT	(0x3UL << TCR_IPS_SHIFT)	/* 4TB */
#define	TCR_IPS_44BIT	(0x4UL << TCR_IPS_SHIFT)	/* 16TB */
#define	TCR_IPS_48BIT	(0x5UL << TCR_IPS_SHIFT)	/* 256TB */
#define	TCR_IPS_52BIT	(0x6UL << TCR_IPS_SHIFT)	/* 4PB */

/*
 * MAIR_EL1 — Memory Attribute Indirection Register
 * Defines memory types referenced by page table entries.
 */
#define	MAIR_ATTR(idx, val)	((uint64_t)(val) << ((idx) * 8))
#define	MAIR_DEVICE_nGnRnE	0x00UL	/* Device non-Gathering, etc. */
#define	MAIR_DEVICE_nGnRE	0x04UL	/* Device non-Gathering, non-Reorder */
#define	MAIR_DEVICE_GRE		0x0CUL	/* Device Gathering, Reorder */
#define	MAIR_NORMAL_NC		0x44UL	/* Normal non-cacheable */
#define	MAIR_NORMAL_WT		0xBBUL	/* Normal write-through */
#define	MAIR_NORMAL_WB		0xFFUL	/* Normal write-back */

/* Standard MAIR index assignments */
#define	MAIR_IDX_DEVICE_nGnRnE	0
#define	MAIR_IDX_NORMAL_NC	1
#define	MAIR_IDX_NORMAL_WT	2
#define	MAIR_IDX_NORMAL_WB	3
#define	MAIR_IDX_DEVICE_nGnRE	4

/*
 * SPSR (Saved Program Status Register) fields
 */
#define	SPSR_MODE_EL0t	0x00UL
#define	SPSR_MODE_EL1t	0x04UL
#define	SPSR_MODE_EL1h	0x05UL
#define	SPSR_MODE_EL2t	0x08UL
#define	SPSR_MODE_EL2h	0x09UL
#define	SPSR_F		(1UL << 6)	/* FIQ mask */
#define	SPSR_I		(1UL << 7)	/* IRQ mask */
#define	SPSR_A		(1UL << 8)	/* SError mask */
#define	SPSR_D		(1UL << 9)	/* Debug mask */
#define	SPSR_DAIF	(SPSR_D | SPSR_A | SPSR_I | SPSR_F)

/*
 * ESR_EL1 — Exception Syndrome Register
 */
#define	ESR_EC_SHIFT	26
#define	ESR_EC_MASK	(0x3fUL << ESR_EC_SHIFT)
#define	ESR_IL		(1UL << 25)	/* Instruction length (32-bit) */
#define	ESR_ISS_MASK	0x1ffffffUL

/* Exception Class (EC) values */
#define	ESR_EC_UNKNOWN		0x00
#define	ESR_EC_WFI		0x01	/* WFI/WFE */
#define	ESR_EC_FP_ASIMD	0x07	/* FP/ASIMD access */
#define	ESR_EC_ILL		0x0E	/* Illegal execution state */
#define	ESR_EC_SVC64		0x15	/* SVC in AArch64 */
#define	ESR_EC_HVC64		0x16	/* HVC in AArch64 */
#define	ESR_EC_SMC64		0x17	/* SMC in AArch64 */
#define	ESR_EC_SYSREG		0x18	/* System register access */
#define	ESR_EC_IABT_LOW		0x20	/* Instr abort from lower EL */
#define	ESR_EC_IABT_CUR		0x21	/* Instr abort from current EL */
#define	ESR_EC_PC_ALIGN		0x22	/* PC alignment */
#define	ESR_EC_DABT_LOW		0x24	/* Data abort from lower EL */
#define	ESR_EC_DABT_CUR		0x25	/* Data abort from current EL */
#define	ESR_EC_SP_ALIGN		0x26	/* SP alignment */
#define	ESR_EC_FP64		0x2C	/* FP exception in AArch64 */
#define	ESR_EC_SERROR		0x2F	/* SError */
#define	ESR_EC_BRK		0x3C	/* BRK instruction */

/* Data/Instruction abort ISS fields */
#define	ESR_ISS_DFSC_MASK	0x3f	/* Data Fault Status Code */
#define	ESR_ISS_WnR		(1UL << 6)	/* Write not Read */
#define	ESR_ISS_CM		(1UL << 8)	/* Cache maintenance */
#define	ESR_ISS_EA		(1UL << 9)	/* External abort */
#define	ESR_ISS_FnV		(1UL << 10)	/* FAR not valid */
#define	ESR_ISS_SET_SHIFT	11		/* Synchronous Error Type */
#define	ESR_ISS_SF		(1UL << 15)	/* Sixty-Four bit reg */
#define	ESR_ISS_SRT_SHIFT	16		/* Syndrome Register Transfer */
#define	ESR_ISS_SRT_MASK	(0x1fUL << ESR_ISS_SRT_SHIFT)
#define	ESR_ISS_SSE		(1UL << 21)	/* Syndrome Sign Extend */
#define	ESR_ISS_SAS_SHIFT	22		/* Syndrome Access Size */
#define	ESR_ISS_SAS_MASK	(0x3UL << ESR_ISS_SAS_SHIFT)
#define	ESR_ISS_ISV		(1UL << 24)	/* Instruction Syndrome Valid */

/* Fault Status Code values (DFSC/IFSC) */
#define	FSC_ADDR_L0		0x00	/* Address size fault, level 0 */
#define	FSC_ADDR_L1		0x01
#define	FSC_ADDR_L2		0x02
#define	FSC_ADDR_L3		0x03
#define	FSC_TRANS_L0		0x04	/* Translation fault, level 0 */
#define	FSC_TRANS_L1		0x05
#define	FSC_TRANS_L2		0x06
#define	FSC_TRANS_L3		0x07
#define	FSC_ACCESS_L1		0x09	/* Access flag fault, level 1 */
#define	FSC_ACCESS_L2		0x0A
#define	FSC_ACCESS_L3		0x0B
#define	FSC_PERM_L1		0x0D	/* Permission fault, level 1 */
#define	FSC_PERM_L2		0x0E
#define	FSC_PERM_L3		0x0F
#define	FSC_SYNC_EXT		0x10	/* Synchronous external abort */
#define	FSC_SYNC_PARITY		0x18	/* Synchronous parity error */
#define	FSC_ALIGN		0x21	/* Alignment fault */
#define	FSC_TLB_CONFLICT	0x30	/* TLB conflict abort */

/*
 * DAIF — Interrupt mask bits (used with MSR DAIFSet/DAIFClr)
 */
#define	DAIF_FIQ	(1UL << 0)
#define	DAIF_IRQ	(1UL << 1)
#define	DAIF_ABT	(1UL << 2)	/* SError/Abort */
#define	DAIF_DBG	(1UL << 3)	/* Debug */
#define	DAIF_ALL	(DAIF_FIQ | DAIF_IRQ | DAIF_ABT | DAIF_DBG)

/*
 * CurrentEL — Current Exception Level
 */
#define	CURRENTEL_EL0	(0x0UL << 2)
#define	CURRENTEL_EL1	(0x1UL << 2)
#define	CURRENTEL_EL2	(0x2UL << 2)
#define	CURRENTEL_EL3	(0x3UL << 2)

/*
 * HCR_EL2 — Hypervisor Configuration Register
 */
#define	HCR_VM		(1UL << 0)	/* Virtualization enable */
#define	HCR_SWIO	(1UL << 1)	/* Set/Way Invalidation Override */
#define	HCR_FMO		(1UL << 3)	/* FIQ mask override */
#define	HCR_IMO		(1UL << 4)	/* IRQ mask override */
#define	HCR_AMO		(1UL << 5)	/* SError mask override */
#define	HCR_TSC		(1UL << 19)	/* Trap SMC */
#define	HCR_RW		(1UL << 31)	/* EL1 is AArch64 */

/*
 * TLBI — TLB Invalidation instruction helpers
 */
#define	tlbi_alle1is()	\
	__asm__ __volatile__("tlbi alle1is" ::: "memory")
#define	tlbi_vmalle1is() \
	__asm__ __volatile__("tlbi vmalle1is" ::: "memory")
#define	tlbi_vale1is(va) \
	__asm__ __volatile__("tlbi vale1is, %0" :: "r" (va) : "memory")
#define	tlbi_vaale1is(va) \
	__asm__ __volatile__("tlbi vaale1is, %0" :: "r" (va) : "memory")

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CONTROLREGS_H */
