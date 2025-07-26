#!/usr/bin/env bash
# Creates a loopback device for a QEMU disk image.

set -euo pipefail
shopt -s inherit_errexit

source "$(dirname $0)/bash/prelude.sh"

if [[ "$#" -eq "1" ]]; then
    IMG_FILE="$1"
else
    IMG_FILE="$REPO_DIR/hd.img"
fi

if [[ ! -f "$IMG_FILE" ]]; then
    log_err "Error: '$IMG_FILE' does not exist or is not a regular file"
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
