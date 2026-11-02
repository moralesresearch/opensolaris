#!/bin/sh
#
# Boot the amd64pc ISO under QEMU and check the serial console for every
# bootstrap milestone marker.
#
#   smoke-qemu.sh <iso>
#
# Environment:
#   QEMU_SYSTEM_X86_64  emulator binary (default qemu-system-x86_64)
#   QEMU_MACHINE        -machine value (default q35)
#   QEMU_SMP            CPU count (default 1)
#   QEMU_FIRMWARE       bios (SeaBIOS) or uefi (OVMF)
#   QEMU_MEDIA          cdrom or disk (the image written to a USB stick)
#   QEMU_OVMF_CODE      OVMF code image (searched if unset)
#   QEMU_OVMF_VARS      OVMF variable store template (searched if unset)
#   SMOKE_TIMEOUT       seconds to wait for BOOT_OK (default 90)
#

set -eu

iso=${1:-../../../../build/amd64pc/opensolaris-x86_64.iso}
qemu=${QEMU_SYSTEM_X86_64:-qemu-system-x86_64}
machine=${QEMU_MACHINE:-q35}
smp=${QEMU_SMP:-1}
firmware=${QEMU_FIRMWARE:-bios}
media=${QEMU_MEDIA:-cdrom}
timeout=${SMOKE_TIMEOUT:-90}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/opensolaris-x86_64.XXXXXX")
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

set --
case "$firmware" in
bios)
	;;
uefi)
	code=${QEMU_OVMF_CODE:-$(find_first \
	    /usr/share/OVMF/OVMF_CODE_4M.fd \
	    /usr/share/OVMF/OVMF_CODE.fd \
	    /usr/share/edk2/ovmf/OVMF_CODE.fd \
	    /usr/share/qemu/edk2-x86_64-code.fd \
	    /opt/homebrew/share/qemu/edk2-x86_64-code.fd || true)}
	vars=${QEMU_OVMF_VARS:-$(find_first \
	    /usr/share/OVMF/OVMF_VARS_4M.fd \
	    /usr/share/OVMF/OVMF_VARS.fd \
	    /usr/share/edk2/ovmf/OVMF_VARS.fd \
	    /usr/share/qemu/edk2-i386-vars.fd || true)}
	if [ -z "$code" ] || [ -z "$vars" ]; then
		echo "smoke-qemu.sh: OVMF firmware not found;" \
		    "install ovmf or set QEMU_OVMF_CODE/QEMU_OVMF_VARS" >&2
		exit 1
	fi
	cp "$vars" "$tmp/vars.fd"
	set -- \
	    -drive "if=pflash,format=raw,unit=0,readonly=on,file=$code" \
	    -drive "if=pflash,format=raw,unit=1,file=$tmp/vars.fd"
	;;
*)
	echo "smoke-qemu.sh: unknown QEMU_FIRMWARE $firmware" >&2
	exit 1
	;;
esac

case "$media" in
cdrom)
	set -- "$@" -drive "file=$iso,media=cdrom,format=raw,readonly=on"
	;;
disk)
	set -- "$@" -drive "file=$iso,format=raw,if=virtio,readonly=on"
	;;
*)
	echo "smoke-qemu.sh: unknown QEMU_MEDIA $media" >&2
	exit 1
	;;
esac

echo "smoke-qemu.sh: $firmware firmware, $media, $smp CPU(s)"
"$qemu" \
	-machine "$machine" \
	-cpu max \
	-smp "$smp" \
	-m 1G \
	-display none \
	-vga std \
	-monitor none \
	-serial "file:$log" \
	-no-reboot \
	"$@" >"$tmp/qemu.out" 2>&1 &
pid=$!

markers="MB2_OK DESCTBL_OK TRAP_OK TSC_OK IRQ_OK PAGE_INIT_OK PAGE_OK
    HAT_INIT_OK HAT_HIGH_OK CONTEXT_OK SYSCALL_OK PAGEFAULT_OK USER_OK
    RELOC_OK COMMON_OK SMP_OK LOCK_OK XCPU_OK"
if [ "$smp" -gt 1 ]; then
	markers="$markers ACPI_OK IPI_OK"
fi

i=0
while [ "$i" -lt $((timeout * 10)) ]; do
	if grep -q 'OpenSolaris/x86_64 BOOT_OK' "$log" 2>/dev/null; then
		for marker in $markers; do
			if ! grep -q "$marker" "$log"; then
				cat "$log" >&2
				echo "x86_64 QEMU smoke test missed $marker" >&2
				exit 1
			fi
		done
		online=$(sed -n 's/.*online CPUs \([0-9]*\) SMP_OK.*/\1/p' "$log")
		if [ "$online" != "$smp" ]; then
			cat "$log" >&2
			echo "x86_64 QEMU smoke test: $online of $smp CPUs" \
			    "online" >&2
			exit 1
		fi
		# Drop GRUB's terminal escape sequences from the transcript.
		sed 's/\x1b\[[0-9;?]*[a-zA-Z]//g' "$log"
		exit 0
	fi
	if grep -q 'panic' "$log" 2>/dev/null; then
		break
	fi
	if ! kill -0 "$pid" 2>/dev/null; then
		break
	fi
	i=$((i + 1))
	sleep 0.1
done

cat "$log" >&2
cat "$tmp/qemu.out" >&2
echo "x86_64 QEMU smoke test did not reach BOOT_OK" >&2
exit 1
