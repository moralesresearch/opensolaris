# OpenSolaris (snv_129) with x86_64 and AArch64 bootstrap ISOs

This tree is the OpenSolaris ON consolidation at build snv_129 (December
2009), plus two bring-up ports. Both build with a stock Clang/LLD toolchain
on Linux or macOS and produce bootable ISOs:

| Port | Directory | Medium | Boots on |
| --- | --- | --- | --- |
| x86_64 | [`usr/src/uts/amd64pc`](usr/src/uts/amd64pc/README.md) | `build/amd64pc/opensolaris-x86_64.iso` | BIOS and UEFI, from CD/DVD or USB stick (GRUB 2, Multiboot2) |
| AArch64 | [`usr/src/uts/arm64pc`](usr/src/uts/arm64pc/README.md) | `build/arm64pc/opensolaris-aarch64.iso` | UEFI with a device tree, from CD/DVD or USB stick (own `BOOTAA64.EFI`) |

## Building the ISOs

On Debian or Ubuntu:

```sh
sudo apt-get install clang lld llvm make python3 xorriso mtools \
    grub-common grub-pc-bin grub-efi-amd64-bin \
    qemu-system-x86 qemu-system-arm ovmf qemu-efi-aarch64
```

Then run:

```sh
make iso      # both ISOs
make smoke    # boot every configuration under QEMU and check the console
```

`make smoke` boots the following configurations:

- x86_64: SeaBIOS CD-ROM with 1 and 4 CPUs, OVMF CD-ROM with 4 CPUs, and
  OVMF from a USB-style disk.
- AArch64: QEMU direct boot (EL1, EL2, and 4 CPUs), AAVMF CD-ROM at EL2
  with 4 CPUs, and AAVMF from a USB-style disk at EL1.

Every run must reach `BOOT_OK` with all milestone markers. The same pipeline
runs in GitHub Actions (`.github/workflows/bootstrap-isos.yml`). Pushing a
tag that matches `iso-v*` publishes both ISOs as a release.

## What the ISOs contain today

Each ISO boots a bootstrap kernel. It's real machine-dependent OpenSolaris
code plus a few unmodified common objects. It brings up the CPU, memory
management, traps, interrupts, user mode and SMP, runs a self-test for each
milestone, and halts. It's not yet a multi-user OpenSolaris system. The
bootstrap stops at the hand-off to `uts/common/os/main.c`, and the
per-port READMEs list the remaining integration steps.

The historical full build (`usr/src/Makefile`, `nightly`, `bldenv`) still
needs a Solaris build host, Sun Studio 12 and the closed binaries.

## Status and roadmap

These are the remaining paths from here to a full installable system,
roughly in order of effort:

1. **Continue this port.** Enter `main()` on both architectures, then bring
   up krtld/genunix modules, a boot archive, ZFS root, and userland
   (libc, SMF, the packaging and installer). The kernel milestones are
   tractable. The userland and packaging work is the largest part: it's
   what illumos spent years modernizing for current compilers.
2. **Rebase on illumos-gate.** illumos is the maintained continuation of
   this code base. It already builds with GCC on x86_64 and has an AArch64
   port in progress. The arm64pc/amd64pc work and the UEFI loader could be
   carried over.
3. **Write a new OS.** The bootstrap layers here (UEFI and Multiboot2 boot,
   descriptor tables, traps, APIC/GIC, page tables, SMP) are a good
   starting point for a new kernel on both architectures. Without
   OpenSolaris's userland and drivers, though, that's a much longer road
   to a usable system.
