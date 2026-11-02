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
 * Multiboot2 boot-information parser.  The historical i86pc dboot spoke
 * Multiboot 1 (uts/common/sys/multiboot.h); GRUB 2's Multiboot2 adds the
 * UEFI memory map, ACPI RSDP and framebuffer descriptions that a modern
 * x86_64 boot needs, and works identically from BIOS and UEFI GRUB.
 */

#include <sys/types.h>
#include <sys/amd64pc.h>

#define	MB2_BOOTLOADER_MAGIC	0x36d76289

#define	MB2_TAG_END		0
#define	MB2_TAG_CMDLINE		1
#define	MB2_TAG_LOADER_NAME	2
#define	MB2_TAG_MMAP		6
#define	MB2_TAG_FRAMEBUFFER	8
#define	MB2_TAG_EFI32		11
#define	MB2_TAG_EFI64		12
#define	MB2_TAG_ACPI_OLD	14
#define	MB2_TAG_ACPI_NEW	15

#define	MB2_MMAP_AVAILABLE	1

struct mb2_tag {
	uint32_t	type;
	uint32_t	size;
};

struct mb2_mmap_entry {
	uint64_t	addr;
	uint64_t	len;
	uint32_t	type;
	uint32_t	zero;
};

struct mb2_tag_mmap {
	uint32_t	type;
	uint32_t	size;
	uint32_t	entry_size;
	uint32_t	entry_version;
};

struct mb2_tag_framebuffer {
	uint32_t	type;
	uint32_t	size;
	uint64_t	addr;
	uint32_t	pitch;
	uint32_t	width;
	uint32_t	height;
	uint8_t		bpp;
	uint8_t		fbtype;
	uint16_t	reserved;
};

int
mb2_parse(uint32_t magic, uint64_t mbi, struct amd64pc_bootinfo *bi)
{
	uint32_t total;
	uint64_t p, end;

	if (magic != MB2_BOOTLOADER_MAGIC || mbi == 0)
		return (-1);
	total = *(volatile uint32_t *)(uintptr_t)mbi;
	bi->bi_mbi = mbi;
	bi->bi_mbi_size = total;
	end = mbi + total;

	for (p = mbi + 8; p + sizeof (struct mb2_tag) <= end;
	    p = (p + ((struct mb2_tag *)(uintptr_t)p)->size + 7) & ~7ULL) {
		struct mb2_tag *tag = (struct mb2_tag *)(uintptr_t)p;

		if (tag->type == MB2_TAG_END || tag->size < 8)
			break;
		switch (tag->type) {
		case MB2_TAG_CMDLINE:
			bi->bi_cmdline = (const char *)(tag + 1);
			break;
		case MB2_TAG_LOADER_NAME:
			bi->bi_loader = (const char *)(tag + 1);
			break;
		case MB2_TAG_MMAP: {
			struct mb2_tag_mmap *mt = (struct mb2_tag_mmap *)tag;
			uint64_t e;

			if (mt->entry_size < sizeof (struct mb2_mmap_entry))
				break;
			for (e = p + sizeof (*mt); e + mt->entry_size <=
			    p + tag->size; e += mt->entry_size) {
				struct mb2_mmap_entry *me =
				    (struct mb2_mmap_entry *)(uintptr_t)e;

				if (me->type != MB2_MMAP_AVAILABLE ||
				    bi->bi_nmem == AMD64PC_MAX_MEMSEGS)
					continue;
				bi->bi_mem[bi->bi_nmem].ms_base = me->addr;
				bi->bi_mem[bi->bi_nmem].ms_size = me->len;
				bi->bi_nmem++;
			}
			break;
		}
		case MB2_TAG_FRAMEBUFFER: {
			struct mb2_tag_framebuffer *ft =
			    (struct mb2_tag_framebuffer *)tag;

			bi->bi_fb.fb_addr = ft->addr;
			bi->bi_fb.fb_pitch = ft->pitch;
			bi->bi_fb.fb_width = ft->width;
			bi->bi_fb.fb_height = ft->height;
			bi->bi_fb.fb_bpp = ft->bpp;
			bi->bi_fb.fb_type = ft->fbtype;
			break;
		}
		case MB2_TAG_EFI32:
		case MB2_TAG_EFI64:
			bi->bi_efi = 1;
			break;
		case MB2_TAG_ACPI_NEW:
			/* Prefer the ACPI 2.0+ RSDP (it carries the XSDT). */
			bi->bi_rsdp = p + sizeof (struct mb2_tag);
			break;
		case MB2_TAG_ACPI_OLD:
			if (bi->bi_rsdp == 0)
				bi->bi_rsdp = p + sizeof (struct mb2_tag);
			break;
		default:
			break;
		}
	}
	return (bi->bi_nmem == 0 ? -1 : 0);
}
