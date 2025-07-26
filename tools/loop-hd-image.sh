#!/usr/bin/env bash
# Creates a loopback device for a QEMU disk image.

set -euo pipefail
shopt -s inherit_errexit

source "$(dirname $0)/bash/prelude.sh"

declare -r DEFAULT_IMG_FILE="$REPO_DIR/hd.img"

declare -a POSARGS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            echo "Usage: $0 [options] [FILE]"
            echo
            echo "Creates a loopback device (e.g., /dev/loop0) with partition scan for the FILE"
            echo "disk image (default: '$DEFAULT_IMG_FILE')."
            echo
            echo "Options:"
            echo "    -h, --help"
            echo "        Print this message and exit."
            echo
            echo "Environment variables:"
            echo "    NO_COLOR"
            echo "        Set to a non-empty string to disable colored output."
            exit 0
            ;;

        -*)
            echo "Unrecognized option: '$1'"
            echo "Use '$0 --help'"
            exit 1
            ;;

        *)
            POSARGS+=("$1")
            shift
            ;;
    esac
done

if [[ ${#POSARGS[@]} -gt 1 ]]; then
    log_err "Unexpected positional arguments: ${POSARGS[@]:1}"
    exit 1
elif [[ ${#POSARGS[@]} -eq 1 ]]; then
    IMG_FILE="$(readlink --canonicalize -- "${POSARGS[0]}")"
else
    IMG_FILE="$REPO_DIR/hd.img"
fi

if [[ ! -f "$IMG_FILE" ]]; then
    log_err "Error: image '$IMG_FILE' does not exist or is not a regular file"
    exit 1
fi

log_info "Creating a loopback device for $IMG_FILE"
LOOPN=$(sudo losetup --partscan --find --show "$IMG_FILE")

if [[ -z "$LOOPN" ]]; then
    log_err "Error: failed to create a loopback device"
    exit 1
fi

log_info "Created $LOOPN."

log_info -e "\nBlock devices:"
lsblk $LOOPN

log_info -e "\nPartition table:"
sudo fdisk -l $LOOPN

log_info "Use the following to detach the created loop device:"
log_info "sudo losetup -d $LOOPN"
