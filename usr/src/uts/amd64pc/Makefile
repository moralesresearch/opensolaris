#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
#
# CDDL HEADER END
#
# Copyright 2026 OpenSolaris x86_64 Contributors.
#
# Freestanding x86_64 bootstrap ("amd64pc") and its BIOS + UEFI hybrid ISO.
#
# Like uts/arm64pc this uses a stock Clang/LLD cross toolchain so it builds
# on any Linux or macOS host; the sources compile against the unmodified
# uts/intel, uts/i86pc and uts/common headers, and link three unmodified
# OpenSolaris objects (list.c, 12x22.c and the amd64 krtld doreloc.c).
#

BUILD?=		../../../../build/amd64pc
CC_CMD?=	clang
LD_CMD?=	ld.lld
READELF_CMD?=	llvm-readelf
GRUB_MKRESCUE?=	grub-mkrescue

TARGET=		$(BUILD)/opensolaris-x86_64
ELF=		$(TARGET).elf
MAP=		$(TARGET).map
ISO=		$(TARGET).iso

#
# -D__sun: the tree's headers (ACPICA's acenv.h in particular) select the
# Solaris environment from the compiler's predefined __sun, which a cross
# compiler hosted on Linux or macOS does not set.
#
INCLUDES=	-I. -I../i86pc -I../intel -I../common -I../../head
DEFINES=	-D__sun -D_KERNEL -D_MACHDEP -D__EXTENSIONS__ -D_ELF64 \
		-DAMD64PC_BOOTSTRAP
TARGET_FLAGS=	--target=x86_64-none-elf -march=x86-64
# -fgnu89-inline: uts/intel/asm/*.h use GNU89 "extern __inline__" bodies.
COMMON_FLAGS=	$(TARGET_FLAGS) -ffreestanding -fno-builtin \
		-fno-stack-protector -fno-pic -fno-pie -mno-red-zone \
		-mno-mmx -mno-sse -mno-sse2 -mno-avx -fgnu89-inline -nostdinc \
		$(DEFINES) $(INCLUDES)
CFLAGS=		$(COMMON_FLAGS) -O2 -g -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unknown-pragmas
# Unmodified OpenSolaris sources: keep their historical warning profile.
CFLAGS_ONTREE=	$(COMMON_FLAGS) -O2 -g -w
ASFLAGS=	$(COMMON_FLAGS) -g

OBJS=	$(BUILD)/dboot_mb2.o \
	$(BUILD)/exception.o \
	$(BUILD)/ap_trampoline.o \
	$(BUILD)/startup.o \
	$(BUILD)/desctbls.o \
	$(BUILD)/trap.o \
	$(BUILD)/user.o \
	$(BUILD)/selftest.o \
	$(BUILD)/mb2.o \
	$(BUILD)/page.o \
	$(BUILD)/hat.o \
	$(BUILD)/smp.o \
	$(BUILD)/cons.o \
	$(BUILD)/acpi.o \
	$(BUILD)/lapic.o \
	$(BUILD)/common_list.o \
	$(BUILD)/font_12x22.o \
	$(BUILD)/krtld_doreloc.o

HEADERS=	sys/amd64pc.h

.PHONY: all clean check iso smoke smoke-iso smoke-all dist

all: $(ELF)

$(BUILD)/.dir:
	mkdir -p $(BUILD)
	touch $@

$(OBJS): $(HEADERS) $(BUILD)/.dir

$(BUILD)/dboot_mb2.o: dboot/dboot_mb2.s
	$(CC_CMD) $(ASFLAGS) -x assembler-with-cpp -c dboot/dboot_mb2.s -o $@

$(BUILD)/exception.o: ml/exception.s
	$(CC_CMD) $(ASFLAGS) -x assembler-with-cpp -c ml/exception.s -o $@

$(BUILD)/ap_trampoline.o: ml/ap_trampoline.s
	$(CC_CMD) $(ASFLAGS) -x assembler-with-cpp -c ml/ap_trampoline.s -o $@

$(BUILD)/startup.o: os/startup.c
	$(CC_CMD) $(CFLAGS) -c os/startup.c -o $@

$(BUILD)/desctbls.o: os/desctbls.c
	$(CC_CMD) $(CFLAGS) -c os/desctbls.c -o $@

$(BUILD)/trap.o: os/trap.c
	$(CC_CMD) $(CFLAGS) -c os/trap.c -o $@

$(BUILD)/user.o: os/user.c
	$(CC_CMD) $(CFLAGS) -c os/user.c -o $@

$(BUILD)/selftest.o: os/selftest.c
	$(CC_CMD) $(CFLAGS) -c os/selftest.c -o $@

$(BUILD)/mb2.o: boot/mb2.c
	$(CC_CMD) $(CFLAGS) -c boot/mb2.c -o $@

$(BUILD)/page.o: boot/page.c
	$(CC_CMD) $(CFLAGS) -c boot/page.c -o $@

$(BUILD)/hat.o: boot/hat.c
	$(CC_CMD) $(CFLAGS) -c boot/hat.c -o $@

$(BUILD)/smp.o: boot/smp.c
	$(CC_CMD) $(CFLAGS) -c boot/smp.c -o $@

$(BUILD)/cons.o: io/cons.c
	$(CC_CMD) $(CFLAGS) -c io/cons.c -o $@

$(BUILD)/acpi.o: io/acpi.c
	$(CC_CMD) $(CFLAGS) -c io/acpi.c -o $@

$(BUILD)/lapic.o: io/lapic.c
	$(CC_CMD) $(CFLAGS) -c io/lapic.c -o $@

$(BUILD)/common_list.o: ../common/os/list.c
	$(CC_CMD) $(CFLAGS_ONTREE) -c ../common/os/list.c -o $@

$(BUILD)/font_12x22.o: ../common/io/12x22.c
	$(CC_CMD) $(CFLAGS_ONTREE) -c ../common/io/12x22.c -o $@

$(BUILD)/krtld_doreloc.o: ../intel/amd64/krtld/doreloc.c
	$(CC_CMD) $(CFLAGS_ONTREE) -I../common/krtld -c \
	    ../intel/amd64/krtld/doreloc.c -o $@

$(ELF): $(OBJS) amd64pc.ld
	$(LD_CMD) -T amd64pc.ld -nostdlib -static -z max-page-size=4096 \
	    -Map=$(MAP) -o $@ $(OBJS)

check: all
	$(READELF_CMD) -h $(ELF) | grep -q 'X86-64'
	$(READELF_CMD) -s $(ELF) | grep -q ' _start$$'
	$(READELF_CMD) -s $(ELF) | grep -q ' amd64pc_main$$'
	$(READELF_CMD) -s $(ELF) | grep -q ' do_reloc_krtld$$'
	./tools/check-mb2.sh $(ELF)

$(ISO): $(ELF) grub.cfg tools/mkiso.sh
	GRUB_MKRESCUE=$(GRUB_MKRESCUE) ./tools/mkiso.sh $(ELF) grub.cfg $@

iso: $(ISO)

#
# Boot the ISO under QEMU: SeaBIOS from CD-ROM (1 and 4 CPUs), OVMF UEFI
# from CD-ROM with 4 CPUs, and OVMF from the same image attached as a disk
# (i.e. written to a USB stick).
#
smoke-iso: $(ISO)
	QEMU_FIRMWARE=bios QEMU_MEDIA=cdrom QEMU_SMP=1 \
	    ./tools/smoke-qemu.sh $(ISO)
	QEMU_FIRMWARE=bios QEMU_MEDIA=cdrom QEMU_SMP=4 \
	    ./tools/smoke-qemu.sh $(ISO)
	QEMU_FIRMWARE=uefi QEMU_MEDIA=cdrom QEMU_SMP=4 \
	    ./tools/smoke-qemu.sh $(ISO)
	QEMU_FIRMWARE=uefi QEMU_MEDIA=disk QEMU_SMP=2 \
	    ./tools/smoke-qemu.sh $(ISO)

smoke: $(ISO)
	./tools/smoke-qemu.sh $(ISO)

smoke-all: smoke-iso

dist: $(ISO)
	tar -czf $(TARGET).tar.gz -C $(BUILD) \
	    opensolaris-x86_64.elf opensolaris-x86_64.map \
	    opensolaris-x86_64.iso

clean:
	rm -rf $(BUILD)
