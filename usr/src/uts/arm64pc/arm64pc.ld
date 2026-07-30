/* Link layout for the freestanding QEMU virt bootstrap image. */
OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
	. = 0x40000000;
	__image_start = .;

	.text.boot : ALIGN(16) {
		KEEP(*(.text.boot))
	}

	.vectors : ALIGN(0x800) {
		KEEP(*(.vectors))
	}

	.text : ALIGN(16) {
		*(.text .text.*)
	}

	.rodata : ALIGN(16) {
		*(.rodata .rodata.*)
	}

	/* Active bootstrap page tables must survive the normal BSS clear. */
	.data.boot : ALIGN(0x1000) {
		*(.data.boot)
	}

	.data : ALIGN(16) {
		*(.data .data.*)
	}

	/* The temporary dboot stack is active before locore clears BSS. */
	.bss.boot (NOLOAD) : ALIGN(16) {
		*(.bss.boot)
	}

	__bss_start = .;
	.bss (NOLOAD) : ALIGN(16) {
		*(.bss .bss.*)
		*(COMMON)
	}
	__bss_end = .;
	__image_end = .;

	/DISCARD/ : {
		*(.comment)
		*(.note .note.*)
		*(.eh_frame .eh_frame.*)
	}
}
