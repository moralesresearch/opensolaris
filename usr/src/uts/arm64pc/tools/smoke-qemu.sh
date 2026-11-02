#!/bin/sh
#
# Boot an arm64pc image under QEMU and check the serial console for every
# bootstrap milestone marker.
#
#   smoke-qemu.sh <image>
#
# <image> is either a flat/ELF kernel, which QEMU loads directly with
# -kernel, or an install ISO, which is booted through real UEFI firmware
# (AAVMF/edk2).  Environment:
#
#   QEMU_SYSTEM_AARCH64  emulator binary (default qemu-system-aarch64)
#   QEMU_MACHINE         -machine value
#   QEMU_SMP             CPU count (default 1)
#   QEMU_MEDIA           ISO attachment: cdrom (virtio-scsi) or disk
#                        (virtio-blk, i.e. the image written to a USB stick)
#   QEMU_EFI_CODE        UEFI firmware code image (searched if unset)
#   QEMU_EFI_VARS        UEFI variable store template (searched if unset)
#   SMOKE_TIMEOUT        seconds to wait for BOOT_OK
#

set -eu

kernel=${1:-../../../../build/arm64pc/opensolaris-aarch64.elf}
qemu=${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}
smp=${QEMU_SMP:-1}
media=${QEMU_MEDIA:-cdrom}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/opensolaris-arm64.XXXXXX")
log=$tmp/console.log
pid=

cleanup()
{
	if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
		kill "$pid" 2>/dev/null || true
		wait "$pid" 2>/dev/null || true
	fi
	rm -rf "$tmp"
}
trap cleanup EXIT HUP INT TERM

find_first()
{
	for f in "$@"; do
		if [ -f "$f" ]; then
			echo "$f"
			return 0
		fi
	done
	return 1
}

case "$kernel" in
*.iso)
	# UEFI firmware publishes the device tree only when ACPI is off.
	machine=${QEMU_MACHINE:-virt,gic-version=3,virtualization=on,acpi=off}
	timeout=${SMOKE_TIMEOUT:-90}
	code=${QEMU_EFI_CODE:-$(find_first \
	    /usr/share/AAVMF/AAVMF_CODE.no-secboot.fd \
	    /usr/share/AAVMF/AAVMF_CODE.fd \
	    /usr/share/qemu-efi-aarch64/QEMU_EFI.fd \
	    /usr/share/edk2/aarch64/QEMU_EFI-pflash.raw \
	    /usr/share/qemu/edk2-aarch64-code.fd \
	    /opt/homebrew/share/qemu/edk2-aarch64-code.fd || true)}
	vars=${QEMU_EFI_VARS:-$(find_first \
	    /usr/share/AAVMF/AAVMF_VARS.fd \
	    /usr/share/edk2/aarch64/vars-template-pflash.raw || true)}
	if [ -z "$code" ]; then
		echo "smoke-qemu.sh: no AArch64 UEFI firmware found;" \
		    "install qemu-efi-aarch64 or set QEMU_EFI_CODE" >&2
		exit 1
	fi
	# Both virt pflash banks must be 64 MiB; pad copies to that size.
	cp "$code" "$tmp/code.fd"
	if [ -n "$vars" ]; then
		cp "$vars" "$tmp/vars.fd"
	else
		: >"$tmp/vars.fd"
	fi
	truncate -s 64M "$tmp/code.fd" "$tmp/vars.fd"
	set -- \
	    -drive "if=pflash,format=raw,readonly=on,file=$tmp/code.fd" \
	    -drive "if=pflash,format=raw,file=$tmp/vars.fd"
	case "$media" in
	cdrom)
		set -- "$@" -device virtio-scsi-pci \
		    -drive "if=none,id=cd0,format=raw,media=cdrom,readonly=on,file=$kernel" \
		    -device scsi-cd,drive=cd0
		;;
	disk)
		set -- "$@" \
		    -drive "if=virtio,format=raw,readonly=on,file=$kernel"
		;;
	*)
		echo "smoke-qemu.sh: unknown QEMU_MEDIA $media" >&2
		exit 1
		;;
	esac
	;;
*)
	machine=${QEMU_MACHINE:-virt,gic-version=3,virtualization=on}
	timeout=${SMOKE_TIMEOUT:-10}
	set -- -kernel "$kernel"
	;;
esac

"$qemu" \
	-machine "$machine" \
	-cpu cortex-a57 \
	-smp "$smp" \
	-m 1G \
	-nographic \
	-monitor none \
	-serial stdio \
	-no-reboot \
	"$@" >"$log" 2>&1 &
pid=$!

i=0
while [ "$i" -lt $((timeout * 10)) ]; do
	if grep -q 'OpenSolaris/AArch64 BOOT_OK' "$log"; then
		for marker in CPU_ABI_OK FDT_OK AUTOVECT_OK HAT_HIGH_OK \
		    PAGE_INIT_OK PAGE_HAT_OK PAGE_OK \
		    SMP_OK SMP_CPU_ABI_OK IPI_OK XCALL_OK LOCK_OK SPL_OK \
		    USER_HAT_OK SYSCALL_OK \
		    PAGEFAULT_OK \
		    CONTEXT_OK SWITCH_OK \
		    KTHREAD_OK \
		    DISP_RESUME_OK \
		    RELOC_OK COMMON_OK IRQ_OK; do
			if ! grep -q "$marker" "$log"; then
				cat "$log" >&2
				echo "AArch64 QEMU smoke test missed $marker" >&2
				exit 1
			fi
		done
		# Strip the firmware's terminal escape sequences.
		sed 's/\x1b\[[0-9;?]*[a-zA-Z]//g' "$log"
		exit 0
	fi
	if ! kill -0 "$pid" 2>/dev/null; then
		break
	fi
	i=$((i + 1))
	sleep 0.1
done

cat "$log" >&2
echo "AArch64 QEMU smoke test did not reach BOOT_OK" >&2
exit 1
