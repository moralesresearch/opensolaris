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
 * Interfaces shared by the freestanding amd64pc bootstrap.  The machine
 * definitions themselves (selectors, descriptors, control registers, the
 * trap frame, the local APIC and the ACPI tables) come from the real
 * uts/intel and uts/i86pc headers.
 */

#ifndef _SYS_AMD64PC_H
#define	_SYS_AMD64PC_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	AMD64PC_MAX_CPUS	16
#define	AMD64PC_MAX_MEMSEGS	32

/* Interrupt vectors used by the bootstrap (above the 32 exceptions). */
#define	AMD64PC_VEC_TIMER	0x20
#define	AMD64PC_VEC_IPI		0xf0
#define	AMD64PC_VEC_SPURIOUS	0xff

/* Real-mode page the AP start-up trampoline is copied into. */
#define	AMD64PC_AP_TRAMPOLINE	0x8000

/* Boot information gathered from the loader and firmware. */
struct amd64pc_memseg {
	uint64_t	ms_base;
	uint64_t	ms_size;
};

struct amd64pc_fb {
	uint64_t	fb_addr;
	uint32_t	fb_pitch;
	uint32_t	fb_width;
	uint32_t	fb_height;
	uint8_t		fb_bpp;
	uint8_t		fb_type;	/* 1 = RGB, 2 = EGA text */
};

struct amd64pc_bootinfo {
	const char	*bi_loader;
	const char	*bi_cmdline;
	uint64_t	bi_rsdp;
	uint64_t	bi_mbi;
	uint64_t	bi_mbi_size;
	struct amd64pc_memseg bi_mem[AMD64PC_MAX_MEMSEGS];
	uint32_t	bi_nmem;
	struct amd64pc_fb bi_fb;
	int		bi_efi;
};

struct amd64pc_platform {
	uint64_t	lapic_base;
	uint32_t	cpu_count;
	uint32_t	cpu_apicid[AMD64PC_MAX_CPUS];
	uint32_t	ioapic_count;
	uint64_t	ioapic_base;
	char		oem[7];
};

extern struct amd64pc_bootinfo amd64pc_bootinfo;
extern struct amd64pc_platform amd64pc_platform;

/* Console */
extern void uart_init(void);
extern void uart_putc(char);
extern void fbcons_init(const struct amd64pc_fb *);
extern void fbcons_putc(char);
extern void cons_puts(const char *);
extern void cons_puthex(uint64_t);
extern void cons_putdec(uint64_t);
extern void amd64pc_panic(const char *) __attribute__((noreturn));

/* Multiboot2 */
extern int mb2_parse(uint32_t, uint64_t, struct amd64pc_bootinfo *);

/* Descriptor tables */
extern void desctbl_init(uint32_t cpu, uint64_t rsp0);
extern void desctbl_load(uint32_t cpu);
extern void desctbl_set_rsp0(uint32_t cpu, uint64_t rsp0);

/* Traps */
struct regs;
extern void amd64pc_trap(struct regs *);
typedef void (*amd64pc_handler_t)(struct regs *);
extern void trap_set_handler(uint_t vector, amd64pc_handler_t);
extern int trap_selftest(void);
extern volatile uint64_t trap_count[256];

/* ACPI and local APIC */
extern int acpi_init(uint64_t rsdp);
extern void lapic_init(void);
extern uint32_t lapic_id(void);
extern void lapic_eoi(void);
extern int lapic_timer_test(void);
extern void lapic_send_ipi(uint32_t apicid, uint32_t icr_low);
extern int lapic_ipi_wait(void);

/* Time */
extern void tsc_calibrate(void);
extern uint64_t tsc_hz;
extern uint64_t amd64pc_gethrtime(void);
extern void tsc_delay_us(uint64_t);

/* Physical pages (common page_t) and the 4-level page tables */
extern int page_boot_init(const struct amd64pc_bootinfo *);
extern struct page *page_boot_alloc(void);
extern void page_boot_free(struct page *);
extern uint64_t page_boot_pa(const struct page *);
extern uint64_t page_boot_total(void);
extern uint64_t page_boot_freecnt(void);
extern int page_selftest(void);

extern int hat_boot_init(void);
extern int hat_boot_map(uint64_t va, uint64_t pa, uint64_t flags);
extern int hat_boot_unmap(uint64_t va);
extern int hat_boot_lookup(uint64_t va, uint64_t *pa);
extern int hat_selftest(void);

/* Context primitives */
typedef struct {
	uint64_t	jb_rbx, jb_rbp, jb_r12, jb_r13, jb_r14, jb_r15;
	uint64_t	jb_rsp, jb_rip;
} amd64pc_jmpbuf_t;

extern int amd64pc_setjmp(amd64pc_jmpbuf_t *) __attribute__((returns_twice));
extern void amd64pc_longjmp(amd64pc_jmpbuf_t *, int)
    __attribute__((noreturn));
extern int context_selftest(void);

/* User mode */
extern int user_selftest(void);

/* SMP */
extern int smp_start(void);
extern uint32_t smp_ncpus_online(void);
extern int smp_selftest(void);
extern uint32_t amd64pc_cpuid(void);

/* krtld */
extern int reloc_selftest(void);

/* uts/common */
extern int common_selftest(void);

/* Port I/O and MSRs */
static inline void
pc_outb(uint16_t port, uint8_t v)
{
	__asm__ __volatile__("outb %0, %1" :: "a"(v), "Nd"(port));
}

static inline uint8_t
pc_inb(uint16_t port)
{
	uint8_t v;

	__asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
	return (v);
}

static inline uint64_t
pc_rdmsr(uint32_t msr)
{
	uint32_t lo, hi;

	__asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
	return (((uint64_t)hi << 32) | lo);
}

static inline void
pc_wrmsr(uint32_t msr, uint64_t v)
{
	__asm__ __volatile__("wrmsr" :: "c"(msr), "a"((uint32_t)v),
	    "d"((uint32_t)(v >> 32)));
}

static inline uint64_t
pc_rdtsc(void)
{
	uint32_t lo, hi;

	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return (((uint64_t)hi << 32) | lo);
}

static inline void
pc_cpuid(uint32_t leaf, uint32_t sub, uint32_t *a, uint32_t *b, uint32_t *c,
    uint32_t *d)
{
	__asm__ __volatile__("cpuid" : "=a"(*a), "=b"(*b), "=c"(*c),
	    "=d"(*d) : "a"(leaf), "c"(sub));
}

static inline uint64_t
pc_getcr3(void)
{
	uint64_t v;

	__asm__ __volatile__("movq %%cr3, %0" : "=r"(v));
	return (v);
}

static inline void
pc_invlpg(uint64_t va)
{
	__asm__ __volatile__("invlpg (%0)" :: "r"(va) : "memory");
}

#ifdef __cplusplus
}
#endif

#endif /* _SYS_AMD64PC_H */
