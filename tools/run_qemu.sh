#!/usr/bin/env bash

set -euo pipefail
shopt -s inherit_errexit

source "$(dirname "$(readlink -f "$0")")/bash/prelude.sh"

ISO_PATH="$REPO_DIR/build/kernel.iso"
HD_PATH="$REPO_DIR/hd.img"
HD_SIZE=4M  # must be understandable by qemu-img
QEMU_BIN=qemu-system-i386

if [[ ! -f $HD_PATH ]]; then
    log_err "Error: '$HD_PATH' does not exist or is not a regular file"
    log_err "See README.txt for instructions on how to create a disk image" \
            "with a root partition"
    exit 1
fi

function macos_focus_qemu() {
    (
        sleep 0.3

        local qemu_pid="$(pgrep -n -x "$QEMU_BIN" || true)"
        [[ -n $qemu_pid ]] || return 0

        osascript >/dev/null 2>&1 <<EOF || true
tell application "System Events"
    set frontmost of first application process whose unix id is $qemu_pid to true
end tell
EOF
    ) &
}

[[ $OSTYPE == darwin* ]] && macos_focus_qemu

"$QEMU_BIN" -cdrom "$ISO_PATH"                                \
            -drive file="$HD_PATH",format=raw,if=none,id=disk \
            -device ahci,id=ahci                              \
            -device ide-hd,drive=disk,bus=ahci.0              \
            -boot order=d                                     \
            -serial stdio                                     \
            -d guest_errors                                   \
            "$@"
