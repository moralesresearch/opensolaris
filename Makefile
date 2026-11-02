#
# Convenience entry point for the reproducible bootstrap images.
#
# The historical ON build (usr/src/Makefile, nightly, bldenv) still needs a
# Solaris build host.  These targets need only Clang/LLD, GRUB 2, xorriso,
# mtools and, for the smoke tests, QEMU with OVMF/AAVMF firmware.  See
# README.md.
#
#   make iso          build/amd64pc/opensolaris-x86_64.iso
#                     build/arm64pc/opensolaris-aarch64.iso
#   make check        build both kernels and run their static checks
#   make smoke        boot every ISO configuration under QEMU
#   make clean
#

.PHONY: all iso check smoke dist clean x86_64 aarch64

all: iso

x86_64:
	$(MAKE) -C usr/src/uts/amd64pc iso

aarch64:
	$(MAKE) -C usr/src/uts/arm64pc iso

iso: x86_64 aarch64

check:
	$(MAKE) -C usr/src/uts/amd64pc check
	$(MAKE) -C usr/src/uts/arm64pc check

smoke:
	$(MAKE) -C usr/src/uts/amd64pc smoke-iso
	$(MAKE) -C usr/src/uts/arm64pc smoke-all smoke-iso

dist:
	$(MAKE) -C usr/src/uts/amd64pc dist
	$(MAKE) -C usr/src/uts/arm64pc dist

clean:
	$(MAKE) -C usr/src/uts/amd64pc clean
	$(MAKE) -C usr/src/uts/arm64pc clean
