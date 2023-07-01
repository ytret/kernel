#!/usr/bin/env bash

set -euxo pipefail

cp -v build/kernel.elf isodir/boot/kernel.elf

if [ -e kernel.iso ]; then
    rm -ifv kernel.iso
fi

grub-mkrescue -o kernel.iso isodir/
