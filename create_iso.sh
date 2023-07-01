#!/usr/bin/env bash

set -euxo pipefail

cp -i build/kernel.elf isodir/boot/kernel.elf

if [ -e kernel.iso ]; then
    rm -i --force kernel.iso
fi

grub-mkrescue -o kernel.iso isodir/
