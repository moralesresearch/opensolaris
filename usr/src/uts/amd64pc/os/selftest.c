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
 * Boot-time checks that unmodified OpenSolaris objects work inside the
 * amd64pc bootstrap:
 *
 *   - uts/common/os/list.c (genunix list_t), linked as-is;
 *   - uts/intel/amd64/krtld/doreloc.c, the amd64 relocation engine that
 *     krtld, ld.so.1 and ld share, linked as-is with the minimal krtld
 *     environment (_kobj_printf, ops, conv_reloc_amd64_type) it expects;
 *   - the kernel setjmp/longjmp context primitives.
 */

#include <sys/types.h>
#include <sys/list.h>
#include <sys/elf.h>
#include <sys/elf_amd64.h>
#include <sys/machelf.h>
#include <sys/amd64pc.h>

/* krtld environment for doreloc.c. */
struct bootops;
struct bootops *ops;
static volatile int kobj_messages;

static void
boot_kobj_printf(void *o, const char *fmt, ...)
{
	(void) o;
	(void) fmt;
	kobj_messages++;
}

void (*_kobj_printf)(void *, const char *, ...) = boot_kobj_printf;

const char *
conv_reloc_amd64_type(Word type)
{
	switch (type) {
	case R_AMD64_64:
		return ("R_AMD64_64");
	case R_AMD64_PC32:
		return ("R_AMD64_PC32");
	case R_AMD64_32:
		return ("R_AMD64_32");
	case R_AMD64_32S:
		return ("R_AMD64_32S");
	default:
		return ("R_AMD64_?");
	}
}

extern int do_reloc_krtld(uchar_t, uchar_t *, Xword *, const char *,
    const char *);

int
reloc_selftest(void)
{
	uint64_t q = 0x10;
	uint32_t w = 0x20;
	uint32_t keep = 0x1234;
	Xword v;

	/* S + A into a 64-bit field (addend already in place). */
	v = 0xffffffff81000000ULL;
	if (do_reloc_krtld(R_AMD64_64, (uchar_t *)&q, &v, "sym", "unix") !=
	    1 || q != 0xffffffff81000010ULL)
		return (-1);

	/* S + A - P that sign-extends fits an R_AMD64_PC32 field. */
	v = (Xword)-0x100;
	if (do_reloc_krtld(R_AMD64_PC32, (uchar_t *)&w, &v, "sym", "unix") !=
	    1 || w != (uint32_t)(0x20 - 0x100))
		return (-2);

	/* A kernel-text address must not "fit" a zero-extended R_AMD64_32. */
	v = 0xfffffffffb800000ULL;
	if (do_reloc_krtld(R_AMD64_32, (uchar_t *)&keep, &v, "sym", "unix") !=
	    0 || keep != 0x1234 || kobj_messages == 0)
		return (-3);
	return (0);
}

struct test_node {
	int		value;
	list_node_t	link;
};

int
common_selftest(void)
{
	list_t list;
	struct test_node n[4], *np;
	int i, sum = 0;

	list_create(&list, sizeof (struct test_node),
	    __builtin_offsetof(struct test_node, link));
	for (i = 0; i < 4; i++) {
		n[i].value = i + 1;
		list_insert_tail(&list, &n[i]);
	}
	list_remove(&list, &n[1]);
	for (np = list_head(&list); np != 0; np = list_next(&list, np))
		sum = sum * 10 + np->value;
	if (sum != 134 || list_is_empty(&list))
		return (-1);
	while ((np = list_remove_head(&list)) != 0)
		;
	list_destroy(&list);
	return (0);
}

static amd64pc_jmpbuf_t ctx_jb;
static volatile int ctx_depth;

static void __attribute__((noinline))
context_deep(int n)
{
	volatile char pad[64];

	pad[0] = (char)n;
	ctx_depth = n + pad[0] - n;
	if (n == 0)
		amd64pc_longjmp(&ctx_jb, 42);
	context_deep(n - 1);
}

int
context_selftest(void)
{
	volatile int callee_saved = 0x1234;
	int rv;

	rv = amd64pc_setjmp(&ctx_jb);
	if (rv == 0) {
		context_deep(8);
		return (-1);
	}
	if (rv != 42 || ctx_depth != 0 || callee_saved != 0x1234)
		return (-2);
	return (0);
}
