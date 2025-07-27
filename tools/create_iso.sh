#!/usr/bin/env bash

set -euo pipefail
shopt -s inherit_errexit

source "$(dirname $0)/bash/prelude.sh"

declare -r DEFAULT_BUILD_DIR="$REPO_DIR/build"
declare OPT_BUILD_DIR="$DEFAULT_BUILD_DIR"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build-dir)
            shift
            if [[ $# -eq 0 ]]; then
                echo "--build-dir: expected a build directory" 1>&2
                exit 1
            fi
            OPT_BUILD_DIR="$1"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo
            echo "Options:"
            echo "    -b, --build-dir DIR"
            echo "        Use DIR as the build directory (default: $DEFAULT_BUILD_DIR)."
            echo "    -h, --help"
            echo "        Print this message and exit."
            exit 0
            ;;
        -*)
            echo "Unrecognized option: '$1'" 1>&2
            exit 1
            ;;
        *)
            echo "Unexpected argument: $1" 1>&2
            exit 1
            ;;
    esac
done

if [[ ! -d "$OPT_BUILD_DIR" ]]; then
    echo "Build directory '$OPT_BUILD_DIR' does not exist." >&2
    exit 1
fi

if ! command grub-mkrescue --version &>/dev/null; then
    if ! command grub2-mkrescue --version &>/dev/null; then
        echo "Could not find grub-mkrescue or grub2-mkrescue"
        exit 1
    else
        declare -r MKRESCUE=grub2-mkrescue
    fi
else
    declare -r MKRESCUE=grub-mkrescue
fi

cp -v "$OPT_BUILD_DIR/kernel" "$ISO_DIR/boot/kernel.elf"
cp -v "$OPT_BUILD_DIR/user/user" "$ISO_DIR/user.elf"

if [ -e "$OPT_BUILD_DIR/kernel.iso" ]; then
    rm -ifv "$OPT_BUILD_DIR/kernel.iso"
fi

$MKRESCUE -o "$OPT_BUILD_DIR/kernel.iso" "$ISO_DIR/"
