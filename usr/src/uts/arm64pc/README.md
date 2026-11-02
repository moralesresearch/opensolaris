# OpenSolaris AArch64 port

This directory contains the AArch64 bring-up path for QEMU's `virt` machine.
The current reproducible milestone is a freestanding OpenSolaris kernel
bootstrap that reaches `OpenSolaris/AArch64 BOOT_OK` on the PL011 serial
console. It produces a bootable flat image, a symbol-bearing ELF image, and
a UEFI-bootable install ISO.

## Build and run

Required tools:

- Clang with the `aarch64-none-elf` and `aarch64-unknown-windows` (UEFI
  PE/COFF) targets, which a stock Clang includes
- LLD (`ld.lld` and `lld-link`) and LLVM objcopy/readelf
- `xorriso` and `mtools` for the ISO
- QEMU `qemu-system-aarch64` and AAVMF/edk2 firmware (`qemu-efi-aarch64`)
  for the smoke tests
- A POSIX make and shell

From the repository root:

```sh
make -C usr/src/uts/arm64pc clean all check smoke-all iso smoke-iso dist
```

Inside a configured OpenSolaris `bldenv`, the legacy `usr/src` Makefile also
provides convenience targets:

```sh
make -C usr/src aarch64-smoke
make -C usr/src aarch64-dist
```

Artifacts are written to `build/arm64pc/`:

- `opensolaris-aarch64.elf` — bootable ELF with symbols
- `opensolaris-aarch64.img` — flat QEMU `-kernel` image
- `opensolaris-aarch64.map` — linker map
- `BOOTAA64.EFI` — UEFI loader (PE/COFF)
- `opensolaris-aarch64.iso` — UEFI install medium (CD-ROM or USB stick)
- `opensolaris-aarch64-qemu-virt.tar.gz` — CI/release bundle

To run manually:

```sh
qemu-system-aarch64 \
  -machine virt,gic-version=3,virtualization=on \
  -cpu cortex-a57 -smp 1 -m 512M -nographic \
  -monitor none -serial stdio -no-reboot \
  -kernel build/arm64pc/opensolaris-aarch64.img
```

To boot the ISO through UEFI firmware:

```sh
qemu-system-aarch64 \
  -machine virt,gic-version=3,virtualization=on,acpi=off \
  -cpu cortex-a57 -smp 4 -m 1G -nographic \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/AAVMF/AAVMF_CODE.no-secboot.fd \
  -drive if=pflash,format=raw,file=AAVMF_VARS.fd \
  -device virtio-scsi-pci -device scsi-cd,drive=cd \
  -drive if=none,id=cd,media=cdrom,format=raw,file=build/arm64pc/opensolaris-aarch64.iso
```

`acpi=off` matters. edk2 publishes the device tree configuration table
only when it isn't exposing ACPI tables, and the bootstrap discovers the
platform from the DTB.

## UEFI install medium

`efi/efiboot.c` is a small, self-contained UEFI application installed as
the removable-media loader `\EFI\BOOT\BOOTAA64.EFI`. It's built with
`clang --target=aarch64-unknown-windows` and `lld-link`, with no
gnu-efi/EDK2 dependency. It does the following:

1. Opens the volume it was started from and reads the ELF kernel from
   `/platform/arm64pc/kernel/aarch64/unix`, the same layout as
   `/platform/i86pc/kernel/amd64/unix` on x86.
2. Claims each `PT_LOAD` segment at its physical address with
   `AllocatePages(AllocateAddress)`. On failure it prints the firmware
   memory map entries in the way.
3. Copies the firmware DTB below 4 GiB, where the kernel's bootmem and
   `page_t` setup reserve it.
4. Calls `ExitBootServices()`, then `efi/efi_enter.S` cleans the written
   ranges to the point of coherency, disables the MMU and caches at EL2 or
   EL1, and enters the kernel with `x0` = DTB. That's the same contract as
   QEMU direct boot, so `dboot` is unchanged.

`tools/mkiso.sh` puts the loader and kernel in a FAT EFI system partition
image. The image serves as the El Torito EFI boot image and is also
appended as GPT partition 2, so the ISO boots from optical media and when
written raw to a USB stick. CI boots it both ways: as a virtio-scsi CD-ROM
at EL2 with four CPUs, and as a virtio-blk disk at EL1.

## Implemented bootstrap surface

- EL2-to-EL1 transition and known early stack
- Correct QEMU `virt` RAM/MMIO identity attributes and MMU enablement
- 2 KiB-aligned EL1 exception vector table and complete GP trap frame
- PL011 serial console
- GICv3 distributor and per-CPU interface initialization
- ARM generic timer discovery and an end-to-end PPI interrupt smoke test
- PSCI 1.x calls through the SMC or HVC conduit advertised by QEMU
- Allocation-free FDT discovery of RAM, PL011, GICv3, CPUs and PSCI conduit
- A RAM-aware boot allocator and TTBR1 high-half kernel mapping
- Page-granular kernel map/lookup/unmap and TTBR0 user address spaces
- A common `page_t` freelist over boot RAM that backs page-table
  allocation, per-page HAT mapping lists (hments with ASIDs) and the
  common `hat_page_setattr()`/`getattr()`/`clrattr()` page attributes
- ASID-tagged context switching and SGI-driven remote TLB invalidation
- PSCI SMP startup, per-CPU stacks and EL2-to-EL1 normalization on APs
- Synchronous GICv3 SGI cross-calls that execute functions on target CPUs
- Shared interrupt handler registration, removal and claim dispatch
- GIC priority-mask-backed PIL/SPL primitives with live masking tests
- AArch64 acquire/release dispatcher locks stressed across four CPUs
- Kernel `setjmp`/`longjmp` context primitives with a boot-time test
- Generated C-layout assembly offsets and real `kthread_t` PCB switching
- Common-dispatcher `resume()` ABI with idle-stack handoff, per-thread resume
  locks, CPU/LWP publication, TPIDR_EL1 update, and PIL restoration
- Real boot `cpu_t`/`t0` state with TPIDR_EL1-backed `curthread`, including
  per-CPU thread state on PSCI-started secondary CPUs
- EL0 entry/return, SVC register ABI and recoverable translation faults
- AArch64 ELF definitions and tested krtld RELA instruction encoders
- A real `uts/common` genunix list object linked and exercised at boot
- Machine ABI headers sufficient to syntax-compile unchanged common `main.c`,
  `cpu.c`, `disp.c`, `thread.c`, `avintr.c`, and `vm_as.c`
- Automated serial-console smoke tests for SMC, HVC and four-CPU boot

## Boundary of the current milestone

The bootstrap deliberately calls `arm64_bootstrap_main()` instead of
`uts/common/os/main.c`. It proves that executable architecture code boots; it
is not yet a multi-user OpenSolaris distribution or installer ISO.

The remaining full-kernel port is tracked by these integration boundaries:

1. Back the tested page-table engine with common `page_t`/kmem allocation,
   HAT mapping lists, page attributes, and `as_fault()` rather than bootmem.
2. Enter `uts/common/os/main.c` and run the common dispatcher using the now
   instantiated `cpu_t`/`kthread_t`, lock, PIL and cross-call ABI.
3. Connect the GIC backend to common `add_avintr()` and the generic timer to
   cyclic/clock infrastructure.
4. Complete syscall table, copyin/copyout, signal, FPU/NEON and `/proc` context
   handling, then wrap the relocation core in krtld's module/symbol walker.
5. Implement root nexus/DDI enumeration, DMA and virtio block/network drivers.
6. Cross-build userland and packages and construct a boot archive/root
   filesystem for the ISO. The UEFI removable-media loader and the ISO
   itself are done.

CI (`.github/workflows/bootstrap-isos.yml`) builds and boots the flat image
and the ISO on every relevant change. Tags matching `iso-v*` or `arm64-v*`
publish the x86_64 and AArch64 ISOs, images and checksums as a GitHub
release.
