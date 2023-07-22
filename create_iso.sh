#!/usr/bin/env bash

set -euxo pipefail

cp -v build/kernel.elf isodir/boot/kernel.elf
cp -v build/user.elf isodir/user.elf

if [ -e kernel.iso ]; then
    rm -ifv kernel.iso
fi

grub-mkrescue -o kernel.iso isodir/
