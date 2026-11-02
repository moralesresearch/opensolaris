#!/bin/sh
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
# Copyright 2026 OpenSolaris AArch64 Contributors.
#
# Build the UEFI-bootable AArch64 install medium.
#
#   mkiso.sh <BOOTAA64.EFI> <unix ELF> <output.iso>
#
# Layout (identical in the ISO9660 tree and in the El Torito EFI image):
#
#   /EFI/BOOT/BOOTAA64.EFI                 removable-media UEFI loader
#   /platform/arm64pc/kernel/aarch64/unix  bootstrap kernel (ELF)
#
# The EFI system partition image is also appended as GPT/MBR partition 2,
# so the same file boots from optical media (El Torito) and when written
# raw to a USB stick or attached as a virtual disk.
#

set -eu

loader=$1
kernel=$2
iso=$3

for tool in xorriso mformat mmd mcopy; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		echo "mkiso.sh: $tool is required (install xorriso and mtools)" >&2
		exit 1
	fi
done

work=$(mktemp -d "${TMPDIR:-/tmp}/opensolaris-arm64-iso.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

root=$work/root
esp=$work/efiboot.img
kdir=platform/arm64pc/kernel/aarch64

mkdir -p "$root/EFI/BOOT" "$root/$kdir" "$root/boot"
cp "$loader" "$root/EFI/BOOT/BOOTAA64.EFI"
cp "$kernel" "$root/$kdir/unix"
cat >"$root/README.TXT" <<'README'
OpenSolaris AArch64 bootstrap medium (arm64pc)

Boots on UEFI AArch64 systems that publish a device tree, e.g.

  qemu-system-aarch64 -machine virt,gic-version=3,acpi=off \
      -cpu cortex-a57 -m 1G -nographic \
      -bios /usr/share/AAVMF/AAVMF_CODE.fd -cdrom opensolaris-aarch64.iso

The console is the PL011 serial port.  See usr/src/uts/arm64pc/README.md.
README

# Size the ESP for its contents plus FAT overhead, minimum 4 MiB.
kb=$(du -k "$loader" "$kernel" | awk '{ s += $1 } END { print s }')
kb=$((kb * 2 + 1024))
[ "$kb" -lt 4096 ] && kb=4096
dd if=/dev/zero of="$esp" bs=1024 count="$kb" 2>/dev/null
mformat -i "$esp" -N 0x05012026 -v OSOL_EFI ::
mmd -i "$esp" ::/EFI ::/EFI/BOOT ::/platform ::/platform/arm64pc \
    ::/platform/arm64pc/kernel ::/$kdir
mcopy -i "$esp" "$loader" ::/EFI/BOOT/BOOTAA64.EFI
mcopy -i "$esp" "$kernel" ::/$kdir/unix
cp "$esp" "$root/boot/efiboot.img"

rm -f "$iso"
xorriso -as mkisofs \
	-quiet \
	-R -J -joliet-long \
	-V OPENSOLARIS_AARCH64 \
	-o "$iso" \
	-e boot/efiboot.img -no-emul-boot \
	-append_partition 2 0xef "$esp" \
	-partition_cyl_align all \
	"$root"

echo "mkiso.sh: wrote $iso"
