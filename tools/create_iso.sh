#!/usr/bin/env bash

set -euo pipefail
shopt -s inherit_errexit

source "$(dirname "$(readlink -f "$0")")/bash/prelude.sh"

declare -r DEFAULT_BUILD_DIR="$PWD"
declare -r DEFAULT_CMDLINE=""
declare -r DEFAULT_ISO_DIR="$REPO_DIR/isodir"
declare -r DEFAULT_KERNEL_ISO="$REPO_DIR/build/kernel.iso"

declare OPT_BUILD_DIR="$DEFAULT_BUILD_DIR"
declare OPT_CMDLINE=""
declare OPT_ISO_DIR="$DEFAULT_ISO_DIR"
declare OPT_KERNEL_ISO="$DEFAULT_KERNEL_ISO"

while [[ $# -gt 0 ]]; do
    case "$1" in
        -b|--build-dir)
            if [[ $# -lt 2 ]]; then
                echo "$1: expected a build directory" 1>&2
                exit 1
            fi
            OPT_BUILD_DIR="$2"
            shift 2
            ;;
        -c|--cmdline)
            if [[ $# -lt 2 ]]; then
                echo "$1: expected a cmdline" 1>&2
                exit 1
            fi
            OPT_CMDLINE="$2"
            shift 2
            ;;
        -i|--iso-dir)
            if [[ $# -lt 2 ]]; then
                echo "$1: expected a directory" 1>&2
                exit 1
            fi
            OPT_ISO_DIR="$2"
            shift 2
            ;;
        -o|--iso)
            if [[ $# -lt 2 ]]; then
                echo "$1: expected a filename" 1>&2
            fi
            OPT_KERNEL_ISO="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo
            echo "Options:"
            echo "    -b, --build-dir BUILDDIR"
            echo "        Use BUILDDIR as the build directory (default: $DEFAULT_BUILD_DIR)."
            echo "    -c, --cmdline TEXT"
            echo "        Append TEXT to the cmdline in limine.conf (default: $DEFAULT_CMDLINE)."
            echo "    -i, --iso-dir ISODIR"
            echo "        Use ISODIR as the base filesystem for the ISO (default: $DEFAULT_ISO_DIR)."
            echo "    -o, --iso ISOPATH"
            echo "        Place the output .iso file at ISOPATH (default: $DEFAULT_KERNEL_ISO)."
            echo "    -h, --help"
            echo "        Print this message and exit."
            exit 0
            ;;
        -*)
            echo "Error: unrecognized option '$1'" 1>&2
            exit 1
            ;;
        *)
            echo "Error: unexpected argument '$1'" 1>&2
            exit 1
            ;;
    esac
done

function gen_bl_conf {
    local -r template="$OPT_ISO_DIR/boot/limine/limine.conf.in"
    local -r target="$OPT_ISO_DIR/boot/limine/limine.conf"

    if [[ ! -f $template ]]; then
        log_err "Error: path '$template' does not exist or is not a regular file"
        exit 1
    fi

    cp -v "$template" "$target"

    if [[ $OPT_CMDLINE == *\;* ]]; then
        log_err "Internal error: please update ${BASH_SOURCE[0]} to use a symbol other than ';' in the 's' command"
        log_err "Reason: cmdline '$OPT_CMDLINE' contains the same symbol"
        exit 1
    fi
    sed -i "s;%CMDLINE%;$OPT_CMDLINE;" "$target"
}

if [[ ! -d "$OPT_BUILD_DIR" ]]; then
    log_err "Build directory '$OPT_BUILD_DIR' does not exist."
    exit 1
fi

if ! command xorriso --version &>/dev/null; then
    log_err "Could not find xorriso"
    exit 1
fi

log_info "Install the kernel and modules"
cp -v "$OPT_BUILD_DIR/kernel" "$ISO_DIR/boot/kernel.elf"
cp -v "$OPT_BUILD_DIR/user/user" "$ISO_DIR/user.elf"
cp -v "$REPO_DIR/lua/kshell.lua" "$ISO_DIR/kshell.lua"

log_info "Generate limine.conf from limine.conf.in"
gen_bl_conf

if [ -e "$OPT_KERNEL_ISO" ]; then
    log_info "Remove the old ISO"
    rm -v "$OPT_KERNEL_ISO"
fi

log_info "Create a bootable ISO"
xorriso -as mkisofs \
    -R -r -J \
    -b boot/limine/limine-bios-cd.bin \
    -no-emul-boot -boot-load-size 4 -boot-info-table \
    "$ISO_DIR" -o "$OPT_KERNEL_ISO"
