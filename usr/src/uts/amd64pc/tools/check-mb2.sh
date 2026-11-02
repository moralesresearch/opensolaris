#!/bin/sh
#
# Verify that the kernel carries a valid Multiboot2 header within the
# first 32 KiB of the file, as GRUB requires.
#
set -eu
elf=$1
python3 -I - "$elf" <<'PY'
import struct, sys
data = open(sys.argv[1], 'rb').read(32768)
for off in range(0, len(data) - 16, 8):
    magic, arch, length, csum = struct.unpack_from('<IIII', data, off)
    if magic == 0xe85250d6 and (magic + arch + length + csum) & 0xffffffff == 0:
        print('multiboot2 header at file offset 0x%x, arch %d, length %d'
              % (off, arch, length))
        sys.exit(0)
sys.exit('no valid multiboot2 header in the first 32 KiB')
PY
