#
# Freestanding AArch64 bootstrap build for the QEMU virt platform.
#
# This Makefile intentionally uses a modern cross Clang/LLD toolchain.  It is
# the reproducible bring-up path; the historical dmake kernel integration can
# consume the same objects once genunix and the AArch64 HAT are available.
#

BUILD?=		../../../../build/arm64pc
CC_CMD?=	clang
LD_CMD?=	ld.lld
OBJCOPY_CMD?=	llvm-objcopy
READELF_CMD?=	llvm-readelf

TARGET=		$(BUILD)/opensolaris-aarch64
ELF=		$(TARGET).elf
IMAGE=		$(TARGET).img
MAP=		$(TARGET).map
ASSYM=		$(BUILD)/assym.h
ASSYM_ASM=	$(BUILD)/genassym.s

INCLUDES=	-I. -I../arm64 -I../common -I../../head
DEFINES=	-D_KERNEL -D_MACHDEP -D__EXTENSIONS__ -DARM64_BOOTSTRAP
TARGET_FLAGS=	--target=aarch64-none-elf -mcpu=cortex-a57
COMMON_FLAGS=	$(TARGET_FLAGS) -ffreestanding -fno-builtin \
		-fno-stack-protector -fno-pic -mstrict-align \
		-mgeneral-regs-only -nostdinc $(DEFINES) $(INCLUDES)
CFLAGS=		$(COMMON_FLAGS) -O2 -g -Wall -Wextra -Werror \
		-Wno-unused-parameter -Wno-unknown-pragmas
ASFLAGS=	$(COMMON_FLAGS) -I$(BUILD) -D_ASM -g

OBJS=	$(BUILD)/dboot_arm64.o \
	$(BUILD)/dboot_startkern.o \
	$(BUILD)/locore.o \
	$(BUILD)/exception_vectors.o \
	$(BUILD)/setjmp.o \
	$(BUILD)/swtch.o \
	$(BUILD)/threadp.o \
	$(BUILD)/usermode.o \
	$(BUILD)/lock_prim.o \
	$(BUILD)/kobj_reloc.o \
	$(BUILD)/common_list.o \
	$(BUILD)/fdt.o \
	$(BUILD)/bootmem.o \
	$(BUILD)/boot_hat.o \
	$(BUILD)/boot_page.o \
	$(BUILD)/hat_test.o \
	$(BUILD)/page_test.o \
	$(BUILD)/user_test.o \
	$(BUILD)/lock_test.o \
	$(BUILD)/spl_test.o \
	$(BUILD)/smp.o \
	$(BUILD)/context_test.o \
	$(BUILD)/thread_test.o \
	$(BUILD)/reloc_test.o \
	$(BUILD)/common_test.o \
	$(BUILD)/boot_main.o \
	$(BUILD)/cpu_bootstrap.o \
	$(BUILD)/mlsetup.o \
	$(BUILD)/startup.o \
	$(BUILD)/spl.o \
	$(BUILD)/trap.o \
	$(BUILD)/intr.o \
	$(BUILD)/pl011.o \
	$(BUILD)/gicv3.o \
	$(BUILD)/arch_timer.o \
	$(BUILD)/psci.o

ARCH_HEADERS=	../arm64/sys/*.h ../arm64/asm/*.h sys/*.h

.PHONY: all clean check common-check smoke smoke-all dist

all: $(ELF) $(IMAGE)

$(BUILD)/.dir:
	mkdir -p $(BUILD)
	touch $@

$(OBJS): $(ARCH_HEADERS)

$(BUILD)/dboot_arm64.o: dboot/dboot_arm64.S $(BUILD)/.dir
	$(CC_CMD) $(ASFLAGS) -c dboot/dboot_arm64.S -o $@

$(BUILD)/dboot_startkern.o: dboot/dboot_startkern.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c dboot/dboot_startkern.c -o $@

$(BUILD)/locore.o: ml/locore.s $(BUILD)/.dir
	$(CC_CMD) $(ASFLAGS) -x assembler-with-cpp -c ml/locore.s -o $@

$(BUILD)/exception_vectors.o: ml/exception_vectors.s $(BUILD)/.dir
	$(CC_CMD) $(ASFLAGS) -x assembler-with-cpp -c ml/exception_vectors.s -o $@

$(BUILD)/setjmp.o: ml/setjmp.s $(BUILD)/.dir
	$(CC_CMD) $(ASFLAGS) -x assembler-with-cpp -c ml/setjmp.s -o $@

$(BUILD)/swtch.o: ml/swtch.s $(ASSYM) $(BUILD)/.dir
	$(CC_CMD) $(ASFLAGS) -x assembler-with-cpp -c ml/swtch.s -o $@

$(BUILD)/threadp.o: ml/threadp.s $(BUILD)/.dir
	$(CC_CMD) $(ASFLAGS) -x assembler-with-cpp -c ml/threadp.s -o $@

$(BUILD)/usermode.o: ml/usermode.s $(BUILD)/.dir
	$(CC_CMD) $(ASFLAGS) -x assembler-with-cpp -c ml/usermode.s -o $@

$(BUILD)/lock_prim.o: ml/lock_prim.s $(BUILD)/.dir
	$(CC_CMD) $(ASFLAGS) -x assembler-with-cpp -c ml/lock_prim.s -o $@

$(BUILD)/kobj_reloc.o: ../arm64/krtld/reloc.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c ../arm64/krtld/reloc.c -o $@

$(BUILD)/common_list.o: ../common/os/list.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c ../common/os/list.c -o $@

$(BUILD)/fdt.o: boot/fdt.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/fdt.c -o $@

$(BUILD)/bootmem.o: boot/bootmem.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/bootmem.c -o $@

$(BUILD)/boot_hat.o: boot/boot_hat.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/boot_hat.c -o $@

$(BUILD)/boot_page.o: boot/boot_page.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/boot_page.c -o $@

$(BUILD)/hat_test.o: boot/hat_test.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/hat_test.c -o $@

$(BUILD)/page_test.o: boot/page_test.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/page_test.c -o $@

$(BUILD)/user_test.o: boot/user_test.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/user_test.c -o $@

$(BUILD)/lock_test.o: boot/lock_test.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/lock_test.c -o $@

$(BUILD)/spl_test.o: boot/spl_test.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/spl_test.c -o $@

$(BUILD)/smp.o: boot/smp.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/smp.c -o $@

$(BUILD)/context_test.o: boot/context_test.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/context_test.c -o $@

$(BUILD)/thread_test.o: boot/thread_test.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/thread_test.c -o $@

$(BUILD)/reloc_test.o: boot/reloc_test.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/reloc_test.c -o $@

$(BUILD)/common_test.o: boot/common_test.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/common_test.c -o $@

$(BUILD)/boot_main.o: boot/boot_main.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c boot/boot_main.c -o $@

$(BUILD)/cpu_bootstrap.o: os/cpu_bootstrap.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c os/cpu_bootstrap.c -o $@

$(BUILD)/mlsetup.o: os/mlsetup.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c os/mlsetup.c -o $@

$(BUILD)/startup.o: os/startup.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c os/startup.c -o $@

$(BUILD)/spl.o: os/spl.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c os/spl.c -o $@

$(BUILD)/trap.o: os/trap.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c os/trap.c -o $@

$(BUILD)/intr.o: io/intr.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c io/intr.c -o $@

$(BUILD)/pl011.o: io/pl011.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c io/pl011.c -o $@

$(BUILD)/gicv3.o: io/gicv3.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c io/gicv3.c -o $@

$(BUILD)/arch_timer.o: io/arch_timer.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c io/arch_timer.c -o $@

$(BUILD)/psci.o: io/psci.c $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -c io/psci.c -o $@

$(ELF): $(OBJS) arm64pc.ld
	$(LD_CMD) -T arm64pc.ld -Map=$(MAP) -o $@ $(OBJS)

$(IMAGE): $(ELF)
	$(OBJCOPY_CMD) -O binary $(ELF) $(IMAGE)

check: all common-check $(ASSYM)
	$(READELF_CMD) -h $(ELF)
	$(READELF_CMD) -s $(ELF) | grep ' _start$$'
	$(READELF_CMD) -s $(ELF) | grep ' arm64_bootstrap_main$$'
	grep '^#define T_PCB ' $(ASSYM)
	grep '^#define CPU_THREAD ' $(ASSYM)

$(ASSYM): ml/genassym.c $(ARCH_HEADERS) $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -D_MACHDEP -D_ELF64 -I../../common -I../../head \
	    -Wno-sign-compare -S ml/genassym.c -o $(ASSYM_ASM)
	./tools/genassym.sh $(ASSYM_ASM) $(ASSYM)

common-check: $(BUILD)/.dir
	$(CC_CMD) $(CFLAGS) -Wno-sign-compare -D_ELF64 \
	    -I../../common -I../../head \
	    -fsyntax-only ../common/os/main.c
	$(CC_CMD) $(CFLAGS) -Wno-sign-compare -Wno-deprecated-non-prototype \
	    -Wno-missing-braces \
	    -D_MACHDEP -D_ELF64 -I../../common -I../../head \
	    -fsyntax-only ../common/io/avintr.c
	$(CC_CMD) $(CFLAGS) -Wno-sign-compare -Wno-tautological-compare \
	    -Wno-parentheses -Wno-deprecated-non-prototype \
	    -Wno-unused-label -Wno-sometimes-uninitialized \
	    -Wno-unused-but-set-variable -D_MACHDEP -D_ELF64 \
	    -I../../common -I../../head -fsyntax-only ../common/vm/vm_as.c
	$(CC_CMD) $(CFLAGS) -Wno-sign-compare \
	    -Wno-deprecated-non-prototype -Wno-missing-braces \
	    -Wno-missing-field-initializers -Wno-tautological-compare \
	    -Wno-parentheses -Wno-unused-label -Wno-sometimes-uninitialized \
	    -Wno-unused-but-set-variable -Wno-unused-variable -D_ELF64 \
	    -I../../common -I../../head -fsyntax-only ../common/os/cpu.c
	$(CC_CMD) $(CFLAGS) -Wno-sign-compare \
	    -Wno-deprecated-non-prototype -Wno-missing-braces \
	    -Wno-missing-field-initializers -Wno-tautological-compare \
	    -Wno-parentheses -Wno-unused-label -Wno-sometimes-uninitialized \
	    -Wno-unused-but-set-variable -Wno-unused-variable -D_ELF64 \
	    -I../../common -I../../head -fsyntax-only ../common/disp/disp.c
	$(CC_CMD) $(CFLAGS) -Wno-sign-compare \
	    -Wno-deprecated-non-prototype -Wno-missing-braces \
	    -Wno-missing-field-initializers -Wno-tautological-compare \
	    -Wno-parentheses -Wno-unused-label -Wno-sometimes-uninitialized \
	    -Wno-unused-but-set-variable -Wno-unused-variable -D_ELF64 \
	    -I../../common -I../../head -fsyntax-only ../common/disp/thread.c

smoke: all
	./tools/smoke-qemu.sh $(IMAGE)

smoke-all: all
	QEMU_MACHINE='virt,gic-version=3,virtualization=on' \
	    ./tools/smoke-qemu.sh $(IMAGE)
	QEMU_MACHINE='virt,gic-version=3' ./tools/smoke-qemu.sh $(IMAGE)
	QEMU_MACHINE='virt,gic-version=3,virtualization=on' QEMU_SMP=4 \
	    ./tools/smoke-qemu.sh $(IMAGE)

dist: all
	tar -czf $(TARGET)-qemu-virt.tar.gz -C $(BUILD) \
	    opensolaris-aarch64.elf opensolaris-aarch64.img \
	    opensolaris-aarch64.map

clean:
	rm -rf $(BUILD)
