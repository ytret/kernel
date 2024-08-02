#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(dirname $0)"
BUILD_DIR="$SCRIPT_DIR/build"
ISO_DIR="$SCRIPT_DIR/isodir"

if ! command grub-mkrescue --version &>/dev/null; then
    if ! command grub2-mkrescue --version &>/dev/null; then
        echo "Could not find grub-mkrescue or grub2-mkrescue"
        exit 1
    else
        MKRESCUE=grub2-mkrescue
    fi
else
    MKRESCUE=grub-mkrescue
fi

cp -v "$BUILD_DIR/kernel" "$ISO_DIR/boot/kernel.elf"
cp -v "$BUILD_DIR/user/user" "$ISO_DIR/user.elf"

if [ -e "$SCRIPT_DIR/kernel.iso" ]; then
    rm -ifv "$SCRIPT_DIR/kernel.iso"
fi

$MKRESCUE -o "$SCRIPT_DIR/kernel.iso" "$ISO_DIR/"
