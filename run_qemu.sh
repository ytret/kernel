#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(dirname $0)"
ISO_PATH="$SCRIPT_DIR/kernel.iso"
HD_PATH="$SCRIPT_DIR/hd.img"
HD_SIZE=4M  # must be understandable by qemu-img

if [[ ! -f $HD_PATH ]]; then
    echo "'$HD_PATH' does not exist or is not a regular file."
    echo "See README.txt for instructions on how to create a disk image with" \
         "a root partition."
    exit 1
fi

qemu-system-i386 -cdrom "$ISO_PATH"                                \
                 -drive file="$HD_PATH",format=raw,if=none,id=disk \
                 -device ahci,id=ahci                              \
                 -device ide-hd,drive=disk,bus=ahci.0              \
                 -boot order=d                                     \
                 -d guest_errors                                   \
                 "$@"
