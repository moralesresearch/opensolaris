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
 * ARM PL011 UART driver.
 * Provides serial console for AArch64 platforms.
 * QEMU virt machine has a PL011 UART at 0x09000000.
 */

#include <sys/types.h>
#include <sys/platform.h>

/*
 * PL011 register offsets.
 */
#define	PL011_DR	0x000	/* Data Register */
#define	PL011_RSR	0x004	/* Receive Status / Error Clear */
#define	PL011_FR	0x018	/* Flag Register */
#define	PL011_ILPR	0x020	/* IrDA Low-Power Counter */
#define	PL011_IBRD	0x024	/* Integer Baud Rate Divisor */
#define	PL011_FBRD	0x028	/* Fractional Baud Rate Divisor */
#define	PL011_LCR_H	0x02C	/* Line Control Register */
#define	PL011_CR	0x030	/* Control Register */
#define	PL011_IFLS	0x034	/* Interrupt FIFO Level Select */
#define	PL011_IMSC	0x038	/* Interrupt Mask Set/Clear */
#define	PL011_RIS	0x03C	/* Raw Interrupt Status */
#define	PL011_MIS	0x040	/* Masked Interrupt Status */
#define	PL011_ICR	0x044	/* Interrupt Clear Register */
#define	PL011_DMACR	0x048	/* DMA Control Register */

/* Flag Register bits */
#define	PL011_FR_TXFF	(1 << 5)	/* TX FIFO full */
#define	PL011_FR_RXFE	(1 << 4)	/* RX FIFO empty */
#define	PL011_FR_BUSY	(1 << 3)	/* UART busy */

/* Control Register bits */
#define	PL011_CR_UARTEN	(1 << 0)	/* UART enable */
#define	PL011_CR_TXE	(1 << 8)	/* TX enable */
#define	PL011_CR_RXE	(1 << 9)	/* RX enable */

/* Line Control bits */
#define	PL011_LCR_WLEN_8	(3 << 5)	/* 8-bit word length */
#define	PL011_LCR_FEN		(1 << 4)	/* FIFO enable */

/* Interrupt bits */
#define	PL011_INT_RX	(1 << 4)	/* Receive interrupt */
#define	PL011_INT_TX	(1 << 5)	/* Transmit interrupt */
#define	PL011_INT_RT	(1 << 6)	/* Receive timeout */

/* UART physical base (QEMU virt) */
#define	PL011_PHYS_BASE	ARM64PC_UART_BASE
#define	PL011_IRQ	ARM64PC_UART_IRQ

static volatile uint8_t *uart_base;

static inline uint32_t
pl011_read(uint32_t offset)
{
	return (*(volatile uint32_t *)(uart_base + offset));
}

static inline void
pl011_write(uint32_t offset, uint32_t val)
{
	*(volatile uint32_t *)(uart_base + offset) = val;
}

/*
 * Initialize the PL011 UART.
 */
void
pl011_init(uint64_t base_addr)
{
	uart_base = (volatile uint8_t *)base_addr;

	/* Disable UART */
	pl011_write(PL011_CR, 0);

	/* Clear all interrupts */
	pl011_write(PL011_ICR, 0x7ff);

	/* Set baud rate (115200 at 24MHz clock → IBRD=13, FBRD=1) */
	/* QEMU doesn't care about baud rate, but set sensible values */
	pl011_write(PL011_IBRD, 13);
	pl011_write(PL011_FBRD, 1);

	/* 8N1, FIFO enabled */
	pl011_write(PL011_LCR_H, PL011_LCR_WLEN_8 | PL011_LCR_FEN);

	/* Enable UART, TX, RX */
	pl011_write(PL011_CR, PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE);
}

/*
 * Write a character to the UART.
 * Waits if TX FIFO is full.
 */
void
pl011_putc(char c)
{
	/* Wait for TX FIFO not full */
	while (pl011_read(PL011_FR) & PL011_FR_TXFF)
		;

	pl011_write(PL011_DR, (uint32_t)c);
}

/*
 * Read a character from the UART.
 * Returns -1 if no character available.
 */
int
pl011_getc(void)
{
	if (pl011_read(PL011_FR) & PL011_FR_RXFE)
		return (-1);

	return ((int)(pl011_read(PL011_DR) & 0xff));
}

/*
 * Check if a character is available.
 */
int
pl011_ischar(void)
{
	return (!(pl011_read(PL011_FR) & PL011_FR_RXFE));
}

/*
 * Write a string to the UART.
 */
void
pl011_puts(const char *s)
{
	while (*s) {
		if (*s == '\n')
			pl011_putc('\r');
		pl011_putc(*s++);
	}
}

/*
 * Enable receive interrupts.
 */
void
pl011_enable_rx_irq(void)
{
	uint32_t imsc = pl011_read(PL011_IMSC);

	imsc |= PL011_INT_RX | PL011_INT_RT;
	pl011_write(PL011_IMSC, imsc);
}

/*
 * Handle UART interrupt.
 * Returns the received character, or -1 if spurious.
 */
int
pl011_intr(void)
{
	uint32_t mis = pl011_read(PL011_MIS);

	if (mis & (PL011_INT_RX | PL011_INT_RT)) {
		/* Clear the interrupt */
		pl011_write(PL011_ICR, PL011_INT_RX | PL011_INT_RT);
		return (pl011_getc());
	}

	return (-1);
}
