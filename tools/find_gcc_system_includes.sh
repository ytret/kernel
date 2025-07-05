#!/usr/bin/env bash
# Finds i686-elf-gcc system include directories.

if ! i686-elf-gcc --version >/dev/null 2>&1; then
  echo "i686-elf-gcc not found in PATH" 1>&2
  echo "script's PATH value: $PATH" 1>&2
  exit 1
fi

GCC_PATH=$(type -P i686-elf-gcc)
GCC_LOCATION=$(dirname "$GCC_PATH")
GCC_VERSION=$(i686-elf-gcc --version | head -1 | awk '{print $3}')

realpath -L "$GCC_LOCATION/../lib/gcc/i686-elf/$GCC_VERSION/include"
