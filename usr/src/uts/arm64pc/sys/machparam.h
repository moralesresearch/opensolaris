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
 * Machine-dependent parameters for AArch64 platform.
 */

#ifndef _SYS_MACHPARAM_H
#define	_SYS_MACHPARAM_H

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Machine-dependent constants.
 * Parallel to i86pc/sys/machparam.h.
 */

/*
 * Maximum number of CPUs supported.
 */
#define	NCPU		256
#define	NCPU_LOG2	8

/*
 * MMU page sizes.
 * AArch64 with 4K granule: 4K, 2M, 1G (3 sizes).
 */
#define	MMU_PAGE_SIZES		3
#define	DEFAULT_MMU_PAGE_SIZES	3

/*
 * Page size definitions.
 * Using 4K granule (standard for server/desktop).
 */
#define	MMU_PAGESIZE	4096		/* 4KB */
#define	MMU_PAGESHIFT	12
#define	MMU_PAGEOFFSET	(MMU_PAGESIZE - 1)
#define	MMU_PAGEMASK	(~MMU_PAGEOFFSET)

/* OpenSolaris logical page size follows the hardware base granule. */
#define	PAGESIZE	MMU_PAGESIZE
#define	PAGESHIFT	MMU_PAGESHIFT
#define	PAGEOFFSET	(PAGESIZE - 1)
#define	PAGEMASK	(~PAGEOFFSET)
#define	DATA_ALIGN	PAGESIZE

#define	DEFAULTSTKSZ_NPGS	5
#define	DEFAULTSTKSZ		(DEFAULTSTKSZ_NPGS * PAGESIZE)

/* Large page (L2 block descriptor): 2MB with 4K granule */
#define	MMU_PAGESIZE_LARGE	(2 * 1024 * 1024)	/* 2MB */
#define	MMU_PAGESHIFT_LARGE	21

/* Huge page (L1 block descriptor): 1GB with 4K granule */
#define	MMU_PAGESIZE_HUGE	(1024 * 1024 * 1024)	/* 1GB */
#define	MMU_PAGESHIFT_HUGE	30

/*
 * KERNELBASE — start of kernel virtual address space.
 * With 48-bit VA and 4K granule, TTBR1 maps 0xFFFF_0000_0000_0000+.
 * We place the kernel at a conventional high address.
 */
#define	KERNELBASE	0xFFFFFFC000000000UL	/* -256GB */

/*
 * Kernel text start address.
 */
#define	KERNEL_TEXT	(KERNELBASE + 0x100000)	/* KERNELBASE + 1MB */

/*
 * Maximum user virtual address (TTBR0 space).
 */
#define	USERLIMIT	0x0000FFFFFFFFFFFFUL	/* Top of 48-bit user space */
#define	USERLIMIT32	0x00000000FFFFF000UL

/*
 * ARGSBASE — base of exec args area (just below USERLIMIT).
 */
#define	ARGSBASE	(USERLIMIT - (1024 * 1024))

/*
 * Maximum number of NUMA nodes.
 */
#define	MAXNODES	16
#define	MAX_MEM_NODES	MAXNODES

/*
 * Cache line size (common for Cortex-A53/A72/A76/Neoverse).
 */
#define	CPU_CACHE_COHERENCE_SIZE	64

/*
 * Clock parameters.
 */
#define	HZ		100		/* clock ticks per second */
#define	TICK		(1000000 / HZ)	/* microseconds per tick */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHPARAM_H */
