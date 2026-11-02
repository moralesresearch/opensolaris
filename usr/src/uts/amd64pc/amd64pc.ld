/*
 * Link layout for the amd64pc bootstrap.  GRUB's Multiboot2 loader places
 * each PT_LOAD segment at its physical address; the kernel runs on dboot's
 * identity map, so virtual and physical addresses are equal.  2 MiB keeps
 * clear of real-mode/BIOS data and of the low pages OVMF reserves.
 */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

SECTIONS
{
	. = 0x200000;
	__image_start = .;

	/* Must lie within the first 32 KiB of the file (Multiboot2 3.1.1). */
	.multiboot2 : ALIGN(8) {
		KEEP(*(.multiboot2))
	}

	.text.boot : ALIGN(16) {
		KEEP(*(.text.boot))
	}

	.text : ALIGN(16) {
		*(.text .text.*)
	}

	.rodata : ALIGN(16) {
		*(.rodata .rodata.*)
	}

	.data : ALIGN(4096) {
		*(.data .data.*)
	}

	__bss_start = .;
	.bss (NOLOAD) : ALIGN(4096) {
		*(.bss .bss.*)
		*(COMMON)
	}
	. = ALIGN(4096);
	__bss_end = .;
	__image_end = .;

	/DISCARD/ : {
		*(.comment)
		*(.note .note.*)
		*(.eh_frame .eh_frame.*)
	}
}
