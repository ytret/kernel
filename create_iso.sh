#!/usr/bin/env bash

set -euo pipefail

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

cp -v build/kernel isodir/boot/kernel.elf
cp -v build/user/user isodir/user.elf

if [ -e kernel.iso ]; then
    rm -ifv kernel.iso
fi

$MKRESCUE -o kernel.iso isodir/
