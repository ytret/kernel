#!/usr/bin/env bash
# Finds i686-elf-gcc system include directories.

GCC_PATH=$(type i686-elf-gcc | sed 's/^i686-elf-gcc is //')
GCC_LOCATION=$(dirname "$GCC_PATH")
GCC_VERSION=$(i686-elf-gcc --version | head -1 | awk '{print $3}')

realpath -L "$GCC_LOCATION/../lib/gcc/i686-elf/$GCC_VERSION/include" |
    tr -d '\n'
