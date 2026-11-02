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
 * OpenSolaris AArch64 UEFI kernel loader (BOOTAA64.EFI).
 *
 * This is the removable-media boot path for the arm64pc bootstrap kernel.
 * It runs as an ordinary UEFI application and:
 *
 *   1. opens the volume it was loaded from (the El Torito EFI system
 *      partition of the install ISO, a USB stick, or a disk ESP);
 *   2. reads the ELF kernel from /platform/arm64pc/kernel/aarch64/unix
 *      (the same layout as /platform/i86pc/kernel/amd64/unix on x86);
 *   3. claims each PT_LOAD segment at its physical load address, copies the
 *      file contents and zero-fills the remainder (BSS);
 *   4. copies the firmware-provided flattened device tree into loader
 *      memory below 4 GiB, which the kernel then keeps reserved;
 *   5. exits boot services and hands control to efi_enter_kernel(), which
 *      cleans the caches, turns the MMU off at the current exception level
 *      and branches to the kernel entry point with x0 = DTB, i.e. exactly
 *      the Linux/QEMU direct-boot convention the kernel already supports.
 */

#include "efi.h"

#define	KERNEL_PATH	u"\\platform\\arm64pc\\kernel\\aarch64\\unix"

#define	EFI_PAGE_SIZE	4096ULL
#define	EFI_PAGES(n)	(((n) + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE)

#define	FDT_MAGIC	0xd00dfeedU

/* ELF64 definitions (kept local: the loader is not a kernel object). */
#define	EI_NIDENT	16
#define	ELFCLASS64	2
#define	ELFDATA2LSB	1
#define	ET_EXEC		2
#define	EM_AARCH64	183
#define	PT_LOAD		1

typedef struct {
	UINT8	e_ident[EI_NIDENT];
	UINT16	e_type;
	UINT16	e_machine;
	UINT32	e_version;
	UINT64	e_entry;
	UINT64	e_phoff;
	UINT64	e_shoff;
	UINT32	e_flags;
	UINT16	e_ehsize;
	UINT16	e_phentsize;
	UINT16	e_phnum;
	UINT16	e_shentsize;
	UINT16	e_shnum;
	UINT16	e_shstrndx;
} Elf64_Ehdr;

typedef struct {
	UINT32	p_type;
	UINT32	p_flags;
	UINT64	p_offset;
	UINT64	p_vaddr;
	UINT64	p_paddr;
	UINT64	p_filesz;
	UINT64	p_memsz;
	UINT64	p_align;
} Elf64_Phdr;

#define	MAX_RANGES	8

struct load_range {
	UINT64	base;
	UINT64	size;
};

extern void efi_enter_kernel(UINT64 entry, UINT64 dtb,
    const struct load_range *ranges, UINT64 nranges)
    __attribute__((noreturn));

static EFI_SYSTEM_TABLE *st;
static EFI_BOOT_SERVICES *bs;
static struct load_range ranges[MAX_RANGES];
static UINT64 nranges;

/*
 * The compiler may lower structure copies and zeroing to these calls even
 * in a freestanding build, so they must exist.
 */
void *
memcpy(void *dst, const void *src, UINTN n)
{
	UINT8 *d = dst;
	const UINT8 *s = src;

	while (n-- != 0)
		*d++ = *s++;
	return (dst);
}

void *
memset(void *dst, int c, UINTN n)
{
	UINT8 *d = dst;

	while (n-- != 0)
		*d++ = (UINT8)c;
	return (dst);
}

static void
puts16(const CHAR16 *s)
{
	st->ConOut->OutputString(st->ConOut, (CHAR16 *)s);
}

static void
puthex(UINT64 v)
{
	static const char digits[] = "0123456789abcdef";
	CHAR16 buf[19];
	int i;

	buf[0] = '0';
	buf[1] = 'x';
	for (i = 0; i < 16; i++)
		buf[i + 2] = digits[(v >> (60 - i * 4)) & 0xf];
	buf[18] = 0;
	puts16(buf);
}

static int
guid_eq(const EFI_GUID *a, const EFI_GUID *b)
{
	const UINT8 *x = (const UINT8 *)a, *y = (const UINT8 *)b;
	UINTN i;

	for (i = 0; i < sizeof (EFI_GUID); i++)
		if (x[i] != y[i])
			return (0);
	return (1);
}

static UINT32
be32(const void *p)
{
	const UINT8 *b = p;

	return (((UINT32)b[0] << 24) | ((UINT32)b[1] << 16) |
	    ((UINT32)b[2] << 8) | b[3]);
}

static EFI_STATUS
fail(const CHAR16 *what, EFI_STATUS status)
{
	puts16(u"efiboot: ");
	puts16(what);
	puts16(u" (status ");
	puthex(status);
	puts16(u")\r\n");
	/* Leave the message on screen before returning to the boot manager. */
	bs->Stall(5 * 1000 * 1000);
	return (EFI_ERROR(status) ? status : EFI_LOAD_ERROR);
}

/*
 * Report every firmware memory descriptor that overlaps [base, base+size)
 * so that an AllocateAddress failure on new firmware is diagnosable from
 * the console alone.
 */
static void
dump_overlaps(UINT64 base, UINT64 size)
{
	UINTN map_size = 0, key, desc_size;
	UINT32 desc_ver;
	EFI_MEMORY_DESCRIPTOR *map = 0;
	UINT8 *p;
	EFI_STATUS s;

	s = bs->GetMemoryMap(&map_size, 0, &key, &desc_size, &desc_ver);
	if (s != EFI_BUFFER_TOO_SMALL)
		return;
	map_size += 4 * desc_size;
	if (EFI_ERROR(bs->AllocatePool(EfiLoaderData, map_size,
	    (void **)&map)))
		return;
	if (EFI_ERROR(bs->GetMemoryMap(&map_size, map, &key, &desc_size,
	    &desc_ver)))
		return;
	for (p = (UINT8 *)map; p < (UINT8 *)map + map_size; p += desc_size) {
		EFI_MEMORY_DESCRIPTOR *d = (EFI_MEMORY_DESCRIPTOR *)p;
		UINT64 end = d->PhysicalStart + d->NumberOfPages * EFI_PAGE_SIZE;

		if (d->PhysicalStart < base + size && end > base) {
			puts16(u"efiboot:   type ");
			puthex(d->Type);
			puts16(u" ");
			puthex(d->PhysicalStart);
			puts16(u"-");
			puthex(end);
			puts16(u"\r\n");
		}
	}
	bs->FreePool(map);
}

static EFI_STATUS
read_kernel(EFI_HANDLE image, void **bufp, UINTN *sizep)
{
	EFI_GUID li_guid = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	EFI_GUID fs_guid = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
	EFI_GUID fi_guid = EFI_FILE_INFO_GUID;
	EFI_LOADED_IMAGE_PROTOCOL *li;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs;
	EFI_FILE_PROTOCOL *root, *file;
	EFI_FILE_INFO *info;
	UINTN info_size, size;
	void *buf;
	EFI_STATUS s;

	s = bs->HandleProtocol(image, &li_guid, (void **)&li);
	if (EFI_ERROR(s))
		return (fail(u"no loaded-image protocol", s));
	s = bs->HandleProtocol(li->DeviceHandle, &fs_guid, (void **)&fs);
	if (EFI_ERROR(s))
		return (fail(u"boot volume has no file system", s));
	s = fs->OpenVolume(fs, &root);
	if (EFI_ERROR(s))
		return (fail(u"cannot open boot volume", s));
	s = root->Open(root, &file, KERNEL_PATH, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(s)) {
		puts16(u"efiboot: missing " KERNEL_PATH u"\r\n");
		return (fail(u"cannot open kernel", s));
	}

	info_size = sizeof (EFI_FILE_INFO) + 512;
	s = bs->AllocatePool(EfiLoaderData, info_size, (void **)&info);
	if (EFI_ERROR(s))
		return (fail(u"out of memory", s));
	s = file->GetInfo(file, &fi_guid, &info_size, info);
	if (EFI_ERROR(s))
		return (fail(u"cannot stat kernel", s));
	size = info->FileSize;
	bs->FreePool(info);

	s = bs->AllocatePool(EfiLoaderData, size, &buf);
	if (EFI_ERROR(s))
		return (fail(u"out of memory for kernel file", s));
	*sizep = size;
	s = file->Read(file, sizep, buf);
	if (EFI_ERROR(s) || *sizep != size)
		return (fail(u"short kernel read", s));
	file->Close(file);
	root->Close(root);
	*bufp = buf;
	return (EFI_SUCCESS);
}

static EFI_STATUS
load_elf(const UINT8 *file, UINTN size, UINT64 *entryp)
{
	const Elf64_Ehdr *eh = (const Elf64_Ehdr *)file;
	UINT16 i;

	if (size < sizeof (*eh) || eh->e_ident[0] != 0x7f ||
	    eh->e_ident[1] != 'E' || eh->e_ident[2] != 'L' ||
	    eh->e_ident[3] != 'F' || eh->e_ident[4] != ELFCLASS64 ||
	    eh->e_ident[5] != ELFDATA2LSB || eh->e_type != ET_EXEC ||
	    eh->e_machine != EM_AARCH64 ||
	    eh->e_phentsize != sizeof (Elf64_Phdr) ||
	    eh->e_phoff + (UINT64)eh->e_phnum * sizeof (Elf64_Phdr) > size)
		return (fail(u"kernel is not an AArch64 ELF64 executable",
		    EFI_LOAD_ERROR));

	for (i = 0; i < eh->e_phnum; i++) {
		const Elf64_Phdr *ph = (const Elf64_Phdr *)
		    (file + eh->e_phoff + (UINT64)i * sizeof (Elf64_Phdr));
		EFI_PHYSICAL_ADDRESS base, end;
		EFI_STATUS s;

		if (ph->p_type != PT_LOAD || ph->p_memsz == 0)
			continue;
		if (ph->p_filesz > ph->p_memsz ||
		    ph->p_offset + ph->p_filesz > size ||
		    nranges == MAX_RANGES)
			return (fail(u"malformed kernel program header",
			    EFI_LOAD_ERROR));

		/*
		 * Segments share pages (the linker packs .rodata right
		 * after .text), so claim page-rounded ranges and skip the
		 * part that an earlier segment already owns.
		 */
		base = ph->p_paddr & ~(EFI_PAGE_SIZE - 1);
		end = (ph->p_paddr + ph->p_memsz + EFI_PAGE_SIZE - 1) &
		    ~(EFI_PAGE_SIZE - 1);
		if (nranges != 0 && base < ranges[nranges - 1].base +
		    ranges[nranges - 1].size)
			base = ranges[nranges - 1].base +
			    ranges[nranges - 1].size;
		if (base < end) {
			EFI_PHYSICAL_ADDRESS want = base;

			s = bs->AllocatePages(AllocateAddress, EfiLoaderData,
			    EFI_PAGES(end - base), &want);
			if (EFI_ERROR(s)) {
				puts16(u"efiboot: kernel range ");
				puthex(base);
				puts16(u"-");
				puthex(end);
				puts16(u" is owned by firmware:\r\n");
				dump_overlaps(base, end - base);
				return (fail(u"cannot claim kernel memory", s));
			}
			if (nranges != 0 && ranges[nranges - 1].base +
			    ranges[nranges - 1].size == base) {
				ranges[nranges - 1].size += end - base;
			} else {
				ranges[nranges].base = base;
				ranges[nranges].size = end - base;
				nranges++;
			}
		}
		memcpy((void *)ph->p_paddr, file + ph->p_offset, ph->p_filesz);
		memset((void *)(ph->p_paddr + ph->p_filesz), 0,
		    ph->p_memsz - ph->p_filesz);
		puts16(u"efiboot: loaded segment ");
		puthex(ph->p_paddr);
		puts16(u" memsz ");
		puthex(ph->p_memsz);
		puts16(u"\r\n");
	}
	if (nranges == 0)
		return (fail(u"kernel has no loadable segments",
		    EFI_LOAD_ERROR));
	*entryp = eh->e_entry;
	return (EFI_SUCCESS);
}

static EFI_STATUS
copy_dtb(UINT64 *dtbp)
{
	EFI_GUID dtb_guid = EFI_DTB_TABLE_GUID;
	EFI_GUID acpi_guid = EFI_ACPI_20_TABLE_GUID;
	EFI_PHYSICAL_ADDRESS copy = 0xffffffffULL;
	const UINT8 *fdt = 0;
	int have_acpi = 0;
	UINT32 size;
	UINTN i;
	EFI_STATUS s;

	for (i = 0; i < st->NumberOfTableEntries; i++) {
		EFI_CONFIGURATION_TABLE *t = &st->ConfigurationTable[i];

		if (guid_eq(&t->VendorGuid, &dtb_guid))
			fdt = t->VendorTable;
		else if (guid_eq(&t->VendorGuid, &acpi_guid))
			have_acpi = 1;
	}
	if (fdt == 0) {
		if (have_acpi)
			puts16(u"efiboot: firmware published ACPI only; on "
			    u"QEMU use -machine virt,acpi=off\r\n");
		return (fail(u"no device tree configuration table",
		    EFI_NOT_FOUND));
	}
	if (be32(fdt) != FDT_MAGIC)
		return (fail(u"device tree has a bad magic", EFI_LOAD_ERROR));
	size = be32(fdt + 4);

	/* The bootstrap identity map covers the low 4 GiB only. */
	s = bs->AllocatePages(AllocateMaxAddress, EfiLoaderData,
	    EFI_PAGES(size), &copy);
	if (EFI_ERROR(s))
		return (fail(u"cannot allocate device tree copy", s));
	memcpy((void *)copy, fdt, size);
	if (nranges == MAX_RANGES)
		return (fail(u"too many load ranges", EFI_LOAD_ERROR));
	ranges[nranges].base = copy;
	ranges[nranges].size = EFI_PAGES(size) * EFI_PAGE_SIZE;
	nranges++;
	puts16(u"efiboot: device tree ");
	puthex(copy);
	puts16(u" size ");
	puthex(size);
	puts16(u"\r\n");
	*dtbp = copy;
	return (EFI_SUCCESS);
}

static EFI_STATUS
exit_boot_services(EFI_HANDLE image)
{
	UINTN map_size = 0, key, desc_size;
	UINT32 desc_ver;
	EFI_MEMORY_DESCRIPTOR *map = 0;
	EFI_STATUS s;
	int tries;

	for (tries = 0; tries < 4; tries++) {
		s = bs->GetMemoryMap(&map_size, map, &key, &desc_size,
		    &desc_ver);
		if (s == EFI_BUFFER_TOO_SMALL) {
			if (map != 0)
				bs->FreePool(map);
			/* The pool allocation itself may add descriptors. */
			map_size += 8 * desc_size;
			s = bs->AllocatePool(EfiLoaderData, map_size,
			    (void **)&map);
			if (EFI_ERROR(s))
				return (s);
			continue;
		}
		if (EFI_ERROR(s))
			return (s);
		/* No console output from here on: it can change the map. */
		s = bs->ExitBootServices(image, key);
		if (!EFI_ERROR(s))
			return (EFI_SUCCESS);
	}
	return (s);
}

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *systab)
{
	void *file;
	UINTN file_size;
	UINT64 entry, dtb;
	EFI_STATUS s;

	st = systab;
	bs = systab->BootServices;
	bs->SetWatchdogTimer(0, 0, 0, 0);

	puts16(u"\r\nOpenSolaris AArch64 EFI loader\r\n");
	puts16(u"efiboot: firmware ");
	puts16(st->FirmwareVendor);
	puts16(u"\r\n");

	if ((s = read_kernel(image, &file, &file_size)) != EFI_SUCCESS)
		return (s);
	puts16(u"efiboot: read " KERNEL_PATH u" (");
	puthex(file_size);
	puts16(u" bytes)\r\n");
	if ((s = load_elf(file, file_size, &entry)) != EFI_SUCCESS)
		return (s);
	if ((s = copy_dtb(&dtb)) != EFI_SUCCESS)
		return (s);
	bs->FreePool(file);

	puts16(u"efiboot: entry ");
	puthex(entry);
	puts16(u", exiting boot services\r\n");
	if ((s = exit_boot_services(image)) != EFI_SUCCESS)
		return (fail(u"ExitBootServices failed", s));

	efi_enter_kernel(entry, dtb, ranges, nranges);
}
