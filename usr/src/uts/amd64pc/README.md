# OpenSolaris x86_64 bootstrap (amd64pc)

This directory holds the x86_64 counterpart of `uts/arm64pc`: a freestanding
OpenSolaris kernel bootstrap built with a stock Clang/LLD cross toolchain,
plus a BIOS + UEFI hybrid install ISO. It reaches
`OpenSolaris/x86_64 BOOT_OK` on the serial console and on the display.

The historical x86 kernel (`uts/i86pc`, `uts/intel`) is still built by the
ON `nightly`/`dmake` gate on a Solaris host with Sun Studio. This bootstrap
doesn't replace it. It shows that the same machine definitions compile with
a modern compiler and run on current firmware, and it gives the port a
bootable medium to grow from.

## Build and run

Required tools:

- Clang and LLD (any recent release; CI uses the Ubuntu 24.04 packages)
- GRUB 2 `grub-mkrescue` with the `i386-pc` and `x86_64-efi` platforms
  (`grub-common grub-pc-bin grub-efi-amd64-bin` on Debian/Ubuntu)
- `xorriso` and `mtools`
- For the smoke tests: `qemu-system-x86_64` and OVMF (`ovmf`)

From the repository root:

```sh
make -C usr/src/uts/amd64pc clean all check iso smoke-iso dist
```

or `make iso` to build both the x86_64 and AArch64 ISOs.

Artifacts are written to `build/amd64pc/`:

- `opensolaris-x86_64.iso` is the hybrid BIOS/UEFI install medium.
- `opensolaris-x86_64.elf` is the kernel (Multiboot2 ELF64, with symbols).
- `opensolaris-x86_64.map` is the linker map.
- `opensolaris-x86_64.tar.gz` is the release bundle.

To run the ISO by hand:

```sh
# BIOS (SeaBIOS)
qemu-system-x86_64 -machine q35 -cpu max -smp 4 -m 1G \
    -serial stdio -cdrom build/amd64pc/opensolaris-x86_64.iso

# UEFI (OVMF)
qemu-system-x86_64 -machine q35 -cpu max -smp 4 -m 1G -serial stdio \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
    -drive if=pflash,format=raw,file=OVMF_VARS_4M.fd \
    -cdrom build/amd64pc/opensolaris-x86_64.iso
```

The ISO can also be written raw to a USB stick (`dd`) and booted on BIOS or
UEFI machines. Secure Boot must be disabled. The console is COM1 at
115200 8N1, mirrored on the framebuffer or EGA text display.

## Medium layout

```
/boot/grub/grub.cfg                      GRUB menu (BIOS and UEFI)
/platform/amd64pc/kernel/amd64/unix      bootstrap kernel
```

## Boot path

1. GRUB 2 loads `unix` through Multiboot2 (`dboot/dboot_mb2.s`). This is
   the modern counterpart of the Multiboot 1 `i86pc/dboot/dboot_grub.s`.
2. dboot clears BSS, checks for long mode, identity-maps the low 4 GiB
   with 2 MiB pages, enables PAE/LME/paging and calls `amd64pc_main()`.
3. `os/startup.c` runs each milestone and prints its marker.

## Implemented bootstrap surface

| Marker | What it proves |
| --- | --- |
| `MB2_OK` | Multiboot2 parsing: memory map, command line, loader, framebuffer, ACPI RSDP, BIOS vs UEFI |
| `DESCTBL_OK` | Per-CPU GDT and TSS plus the IDT, laid out with the real `segments.h` selectors (`KCS_SEL`, `UCS_SEL`, `KTSS_SEL`, IST1 for `#DF`) and the real `set_usegd`/`set_syssegd`/`set_gatesegd` |
| `TRAP_OK` | 256 vectors into a full `struct regs` frame (`intel/amd64/sys/privregs.h`); `#BP`, `#UD` and a recovered kernel `#PF` |
| `ACPI_OK` | RSDP, XSDT/RSDT and MADT walk using the in-tree ACPICA table definitions |
| `TSC_OK` | PIT-calibrated TSC, `gethrtime`-style nanosecond clock |
| `IRQ_OK` | 8259s remapped and masked; local APIC (xAPIC or x2APIC) timer interrupts with EOI |
| `PAGE_INIT_OK`, `PAGE_OK` | Physical memory described by the common `page_t` (`vm/page.h`) with a freelist |
| `HAT_INIT_OK`, `HAT_HIGH_OK` | 4-level page tables built from `page_t` pages; high-half alias map, lookup, unmap and fault |
| `CONTEXT_OK` | Kernel `setjmp`/`longjmp` |
| `SYSCALL_OK`, `PAGEFAULT_OK`, `USER_OK` | Ring 3 via `iretq`; `SYSCALL`/`SYSRET` round trip using the Solaris STAR selector layout; user `#PF` taken on the TSS `rsp0` stack |
| `RELOC_OK` | The unmodified amd64 krtld relocation engine `uts/intel/amd64/krtld/doreloc.c` |
| `COMMON_OK` | The unmodified genunix `uts/common/os/list.c` |
| `SMP_OK`, `IPI_OK`, `LOCK_OK`, `XCPU_OK` | INIT-SIPI-SIPI AP start-up through a real-mode trampoline, fixed-vector IPIs, and a `lock_t` (`machlock.h` ABI) contended by every CPU |

The framebuffer console draws with the original Sun Gallant 12x22 font from
`uts/common/io/12x22.c`, linked unchanged. It handles 16, 24 and 32 bpp
VBE/GOP framebuffers as well as EGA text.

CI boots the ISO four ways: SeaBIOS CD-ROM with 1 and 4 CPUs, OVMF CD-ROM
with 4 CPUs, and OVMF from the image attached as a disk.

## Modernization notes

These are the issues found while compiling unmodified OpenSolaris headers
and sources with Clang 18 on a non-Solaris host:

- `-D__sun` is needed. ACPICA's `acenv.h` and other headers select the
  Solaris environment from the compiler-predefined `__sun`.
- `-fgnu89-inline` is needed. `uts/intel/asm/*.h` define `extern __inline__`
  bodies with GNU89 semantics. Under C99 rules every includer emits a
  global copy, which causes duplicate `getcr8`/`setcr8` symbols.
- Kernel helpers such as `outb`, `inb`, `rdmsr`, `wrmsr` and `tsc_read` are
  already declared by the real headers. Bootstrap inlines use a `pc_`
  prefix so they don't clash with them.

## Boundary of the current milestone

Like arm64pc, the bootstrap doesn't enter `uts/common/os/main.c`. The next
integration boundaries are:

1. Replace the boot page-table engine with `i86pc/vm/hat_i86.c` and
   `htable.c` on top of the common `page_t`/kmem allocators.
2. Enter `main()` with real `cpu_t`/`kthread_t` state. This needs
   `mlsetup()`, `startup()` and the dispatcher, which in turn need the
   `ia32/ml/*.s` primitives (`swtch.s`, `lock_prim.s`, `copy.s`,
   `locore.s`, and others) adapted so the LLVM/GNU assembler accepts them.
   They're written for the Sun assembler and its `/` comment syntax.
3. Connect `pcplusmp`/`apix` and the ACPICA module for interrupt routing.
4. Build krtld, `genunix` and `unix` as real modules with a boot archive,
   then add a root filesystem and userland.
