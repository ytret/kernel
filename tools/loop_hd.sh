#!/usr/bin/env bash
# Creates a loopback device for a QEMU disk image.
#
# To disable color output, set the environment variable NOCOLOR to something,
# for example:
#
#   NOCOLOR=1 ./loop_hd.sh

if [[ -z "$NOCOLOR" ]]; then
    CINFO="\033[0;32m"
    CERR="\033[0;31m"
    CNONE="\033[0m"
else
    CINFO=""
    CERR=""
    CNONE=""
fi
function e_info {
    echo -e "${CINFO}$1${CNONE}"
}
function e_err {
    echo -e "${CERR}$1${CNONE}" 1>&2
}

TOOLS_DIR="$(dirname $(readlink --canonicalize $0))"
ROOT_DIR="$(readlink --canonicalize "$TOOLS_DIR/..")"

if [[ "$#" -eq "1" ]]; then
    HD_IMG="$1"
else
    HD_IMG="$ROOT_DIR/hd.img"
fi

if [[ ! -f "$HD_IMG" ]]; then
    e_err "Error: '$HD_IMG' does not exist or is not a regular file"
    exit 1
fi

e_info "Creating a loopback device for $HD_IMG"
LOOPN=$(sudo losetup --partscan --find --show "$HD_IMG")

if [[ -z "$LOOPN" ]]; then
    e_err "Error: failed to create a loopback device"
    exit 1
fi

e_info "Created $LOOPN."

echo
e_info "Block devices:"
lsblk $LOOPN

echo
e_info "Partition table:"
sudo fdisk -l $LOOPN

echo
e_info "Use the following to detach the created loop device:"
e_info "sudo losetup -d $LOOPN"
