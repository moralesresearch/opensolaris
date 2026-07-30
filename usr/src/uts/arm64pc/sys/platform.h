/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * CDDL HEADER END
 */

/* Copyright 2026 OpenSolaris AArch64 Contributors. */

#ifndef _SYS_ARM64PC_PLATFORM_H
#define	_SYS_ARM64PC_PLATFORM_H

/* Fixed addresses supplied by QEMU's virt machine. */
#define	ARM64PC_RAM_BASE	0x40000000ULL
#define	ARM64PC_GICD_BASE	0x08000000ULL
#define	ARM64PC_GICR_BASE	0x080A0000ULL
#define	ARM64PC_UART_BASE	0x09000000ULL
#define	ARM64PC_UART_IRQ	33
#define	ARM64PC_TIMER_IRQ	30
#define	ARM64PC_XCALL_IRQ	1

#endif /* _SYS_ARM64PC_PLATFORM_H */
