#!/usr/bin/env bash
# Finds i686-elf-gcc system include directories.

set -euo pipefail
shopt -s inherit_errexit

if ! i686-elf-gcc --version >/dev/null 2>&1; then
    echo "Error: i686-elf-gcc not found in PATH" 1>&2
    echo "Error: script's PATH value: $PATH" 1>&2
    exit 1
fi

if ! awk --version >/dev/null 2>&1; then
    echo "Error: awk not found in PATH" 1>&2
    echo "Error: script's PATH value: $PATH" 1>&2
    exit 1
fi

GCC_PATH=$(type -P i686-elf-gcc)
GCC_DIR=$(dirname -- "$GCC_PATH")
GCC_VERSION=$(i686-elf-gcc --version | head -1 | awk '{print $3}')

readlink --canonicalize "$GCC_DIR/../lib/gcc/i686-elf/$GCC_VERSION/include"
