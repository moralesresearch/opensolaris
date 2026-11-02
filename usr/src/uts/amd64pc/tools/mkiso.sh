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
# Copyright 2026 OpenSolaris x86_64 Contributors.
#
# Build the BIOS + UEFI hybrid x86_64 install medium with GRUB 2.
#
#   mkiso.sh <unix ELF> <grub.cfg> <output.iso>
#
# grub-mkrescue produces one image that boots from optical media on BIOS
# (El Torito, i386-pc) and UEFI (El Torito EFI system partition,
# x86_64-efi), and from a USB stick (MBR/GPT hybrid) on either.
#

set -eu

kernel=$1
cfg=$2
iso=$3
mkrescue=${GRUB_MKRESCUE:-grub-mkrescue}

if ! command -v "$mkrescue" >/dev/null 2>&1; then
	# Fedora and others name it grub2-mkrescue.
	if command -v grub2-mkrescue >/dev/null 2>&1; then
		mkrescue=grub2-mkrescue
	else
		echo "mkiso.sh: grub-mkrescue is required" \
		    "(install grub-common, grub-pc-bin, grub-efi-amd64-bin)" >&2
		exit 1
	fi
fi
for tool in xorriso mformat; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		echo "mkiso.sh: $tool is required (install xorriso and mtools)" >&2
		exit 1
	fi
done

libdir=${GRUB_LIBDIR:-/usr/lib/grub}
[ -d "$libdir" ] || libdir=/usr/lib/grub2
missing=0
for p in i386-pc x86_64-efi; do
	if [ ! -d "$libdir/$p" ]; then
		missing=1
		echo "mkiso.sh: warning: GRUB $p modules not found in $libdir;" \
		    "the ISO will not boot on that firmware" >&2
	fi
done
if [ "$missing" -ne 0 ] && [ -n "${REQUIRE_ALL_PLATFORMS:-}" ]; then
	exit 1
fi

work=$(mktemp -d "${TMPDIR:-/tmp}/opensolaris-x86_64-iso.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

root=$work/root
kdir=platform/amd64pc/kernel/amd64
mkdir -p "$root/boot/grub" "$root/$kdir"
cp "$kernel" "$root/$kdir/unix"
cp "$cfg" "$root/boot/grub/grub.cfg"
cat >"$root/README.TXT" <<'README'
OpenSolaris x86_64 bootstrap medium (amd64pc)

Boots on BIOS and UEFI x86_64 systems through GRUB 2 / Multiboot2, from
optical media or written raw to a USB stick, e.g.

  qemu-system-x86_64 -machine q35 -m 1G -smp 4 -serial stdio \
      -cdrom opensolaris-x86_64.iso

The console is COM1 (115200 8N1) plus the display.  See
usr/src/uts/amd64pc/README.md.
README

rm -f "$iso"
# grub-mkrescue builds every platform installed under its library dir.
"$mkrescue" -o "$iso" "$root" -- -volid OPENSOLARIS_X86_64 \
    >"$work/mkrescue.log" 2>&1 || {
	cat "$work/mkrescue.log" >&2
	exit 1
}
echo "mkiso.sh: wrote $iso"
