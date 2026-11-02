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
 * Bootstrap console: an NS16550 UART on COM1 plus the display GRUB hands
 * over, either the EGA text buffer or a 16, 24 or 32 bpp linear
 * framebuffer (VBE or UEFI GOP).  Framebuffer glyphs come from the real
 * OpenSolaris terminal emulator font, uts/common/io/12x22.c (Sun
 * Gallant), linked unchanged.
 */

#include <sys/types.h>
#include <sys/font.h>
#include <sys/amd64pc.h>

#define	COM1		0x3f8
#define	UART_THR	0
#define	UART_IER	1
#define	UART_FCR	2
#define	UART_LCR	3
#define	UART_MCR	4
#define	UART_LSR	5
#define	UART_LSR_THRE	0x20
#define	UART_LCR_DLAB	0x80

extern bitmap_data_t font_data_12x22;

static int uart_present;
static volatile uint32_t cons_lock;

void
uart_init(void)
{
	pc_outb(COM1 + UART_IER, 0x00);
	pc_outb(COM1 + UART_LCR, UART_LCR_DLAB);
	pc_outb(COM1 + 0, 1);		/* divisor 1: 115200 baud */
	pc_outb(COM1 + 1, 0);
	pc_outb(COM1 + UART_LCR, 0x03);	/* 8N1 */
	pc_outb(COM1 + UART_FCR, 0xc7);	/* enable + clear FIFOs */
	pc_outb(COM1 + UART_MCR, 0x0b);	/* DTR, RTS, OUT2 */
	/* A floating bus reads back 0xff: treat that as no UART. */
	uart_present = pc_inb(COM1 + UART_LSR) != 0xff;
}

void
uart_putc(char c)
{
	int spin = 100000;

	if (!uart_present)
		return;
	while ((pc_inb(COM1 + UART_LSR) & UART_LSR_THRE) == 0 && --spin > 0)
		;
	pc_outb(COM1 + UART_THR, (uint8_t)c);
}

/* Display state. */
static struct amd64pc_fb fb;
static uint32_t fb_cols, fb_rows, fb_col, fb_row;
static int fb_ready;

#define	FG_RGB		0x00d0d0d0
#define	BG_RGB		0x00000000
#define	EGA_ATTR	0x07

/*
 * Store one pixel.  The console only uses black and an equal-component
 * grey, so the RGB field order reported by the loader does not matter.
 */
static void
fb_pixel(uint32_t x, uint32_t y, uint32_t rgb)
{
	volatile uint8_t *p = (volatile uint8_t *)(uintptr_t)(fb.fb_addr +
	    (uint64_t)y * fb.fb_pitch + (uint64_t)x * (fb.fb_bpp / 8));

	switch (fb.fb_bpp) {
	case 32:
		*(volatile uint32_t *)p = rgb;
		break;
	case 24:
		p[0] = rgb & 0xff;
		p[1] = (rgb >> 8) & 0xff;
		p[2] = (rgb >> 16) & 0xff;
		break;
	case 16:
		*(volatile uint16_t *)p = (uint16_t)(((rgb >> 8) & 0xf800) |
		    ((rgb >> 5) & 0x07e0) | ((rgb >> 3) & 0x001f));
		break;
	}
}

void
fbcons_init(const struct amd64pc_fb *info)
{
	fb = *info;
	if (fb.fb_addr == 0)
		return;
	if (fb.fb_type == 2) {
		fb_cols = fb.fb_width;
		fb_rows = fb.fb_height;
	} else if (fb.fb_type == 1 && (fb.fb_bpp == 32 || fb.fb_bpp == 24 ||
	    fb.fb_bpp == 16)) {
		fb_cols = fb.fb_width / font_data_12x22.width;
		fb_rows = fb.fb_height / font_data_12x22.height;
	} else {
		return;
	}
	if (fb_cols == 0 || fb_rows == 0 || fb.fb_addr >= (4ULL << 30))
		return;
	fb_col = fb_row = 0;
	fb_ready = 1;
	if (fb.fb_type == 2) {
		volatile uint16_t *ega = (volatile uint16_t *)(uintptr_t)
		    fb.fb_addr;
		uint32_t i;

		for (i = 0; i < fb_cols * fb_rows; i++)
			ega[i] = (EGA_ATTR << 8) | ' ';
	} else {
		uint32_t y, x;

		for (y = 0; y < fb.fb_height; y++)
			for (x = 0; x < fb.fb_width; x++)
				fb_pixel(x, y, BG_RGB);
	}
}

static void
fb_glyph(uint32_t col, uint32_t row, uint8_t ch)
{
	const bitmap_data_t *f = &font_data_12x22;
	const uint8_t *glyph = f->encoding[ch];
	uint32_t bytes_per_row = (f->width + 7) / 8;
	uint32_t y, x;

	for (y = 0; y < (uint32_t)f->height; y++) {
		for (x = 0; x < (uint32_t)f->width; x++) {
			uint8_t bits = glyph != 0 ?
			    glyph[y * bytes_per_row + x / 8] : 0;

			fb_pixel(col * f->width + x, row * f->height + y,
			    (bits & (0x80 >> (x & 7))) ? FG_RGB : BG_RGB);
		}
	}
}

static void
fb_scroll(void)
{
	uint64_t line, lines;
	uint32_t c;

	if (fb.fb_type == 2) {
		volatile uint16_t *ega = (volatile uint16_t *)(uintptr_t)
		    fb.fb_addr;

		for (line = 0; line < (uint64_t)(fb_rows - 1) * fb_cols; line++)
			ega[line] = ega[line + fb_cols];
		for (c = 0; c < fb_cols; c++)
			ega[(fb_rows - 1) * fb_cols + c] =
			    (EGA_ATTR << 8) | ' ';
		return;
	}
	lines = (uint64_t)font_data_12x22.height;
	for (line = 0; line < (uint64_t)(fb_rows - 1) * lines; line++) {
		volatile uint8_t *dst = (volatile uint8_t *)(uintptr_t)
		    (fb.fb_addr + line * fb.fb_pitch);
		volatile uint8_t *src = (volatile uint8_t *)(uintptr_t)
		    (fb.fb_addr + (line + lines) * fb.fb_pitch);
		uint64_t i, n = (uint64_t)fb.fb_width * (fb.fb_bpp / 8);

		if ((n & 3) == 0) {
			for (i = 0; i < n; i += 4)
				*(volatile uint32_t *)(dst + i) =
				    *(volatile uint32_t *)(src + i);
		} else {
			for (i = 0; i < n; i++)
				dst[i] = src[i];
		}
	}
	for (c = 0; c < fb_cols; c++)
		fb_glyph(c, fb_rows - 1, ' ');
}

void
fbcons_putc(char ch)
{
	if (!fb_ready)
		return;
	if (ch == '\r') {
		fb_col = 0;
		return;
	}
	if (ch == '\n') {
		fb_col = 0;
		if (++fb_row == fb_rows) {
			fb_scroll();
			fb_row = fb_rows - 1;
		}
		return;
	}
	if (fb.fb_type == 2) {
		volatile uint16_t *ega = (volatile uint16_t *)(uintptr_t)
		    fb.fb_addr;

		ega[fb_row * fb_cols + fb_col] = (EGA_ATTR << 8) | (uint8_t)ch;
	} else {
		fb_glyph(fb_col, fb_row, (uint8_t)ch);
	}
	if (++fb_col == fb_cols)
		fbcons_putc('\n');
}

static void
cons_lock_enter(void)
{
	while (__atomic_exchange_n(&cons_lock, 1, __ATOMIC_ACQUIRE) != 0)
		__asm__ __volatile__("pause");
}

static void
cons_lock_exit(void)
{
	__atomic_store_n(&cons_lock, 0, __ATOMIC_RELEASE);
}

void
cons_puts(const char *s)
{
	cons_lock_enter();
	for (; *s != '\0'; s++) {
		if (*s == '\n')
			uart_putc('\r');
		uart_putc(*s);
		fbcons_putc(*s);
	}
	cons_lock_exit();
}

void
cons_puthex(uint64_t v)
{
	static const char digits[] = "0123456789abcdef";
	char buf[19];
	int i;

	buf[0] = '0';
	buf[1] = 'x';
	for (i = 0; i < 16; i++)
		buf[i + 2] = digits[(v >> (60 - i * 4)) & 0xf];
	buf[18] = '\0';
	cons_puts(buf);
}

void
cons_putdec(uint64_t v)
{
	char buf[21];
	int i = 20;

	buf[i] = '\0';
	do {
		buf[--i] = (char)('0' + v % 10);
		v /= 10;
	} while (v != 0);
	cons_puts(&buf[i]);
}

void
amd64pc_panic(const char *msg)
{
	cons_puts("\namd64pc: panic: ");
	cons_puts(msg);
	cons_puts("\n");
	for (;;)
		__asm__ __volatile__("cli; hlt");
}
