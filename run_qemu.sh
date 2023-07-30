#!/usr/bin/env bash

set -euo pipefail

HD_PATH=./hd.img
HD_SIZE=4M  # must be understandable by qemu-img

if [[ ! -f $HD_PATH ]]; then
    echo "'$HD_PATH' does not exist or is not a regular file."
    read -p "Create a new hard disk at '$HD_PATH' of size $HD_SIZE? [Y/n] " -n 1 -r
    echo

    if [[ $REPLY =~ ^[Yy]$ ]]; then
        qemu-img create $HD_PATH $HD_SIZE
    else
        exit 0
    fi
fi

qemu-system-i386 -cdrom kernel.iso                               \
                 -drive file=$HD_PATH,format=raw,if=none,id=disk \
                 -device ahci,id=ahci                            \
                 -device ide-hd,drive=disk,bus=ahci.0            \
                 -boot order=d                                   \
                 -d guest_errors                                 \
                 "$@"
