#!/usr/bin/env bash

set -euo pipefail
shopt -s inherit_errexit

source "$(dirname "$(readlink -f "$0")")/bash/prelude.sh"

ISO_PATH="$REPO_DIR/build/kernel.iso"
HD_PATH="$REPO_DIR/hd.img"
HD_SIZE=4M  # must be understandable by qemu-img

if [[ ! -f $HD_PATH ]]; then
    log_err "Error: '$HD_PATH' does not exist or is not a regular file"
    log_err "See README.txt for instructions on how to create a disk image" \
            "with a root partition"
    exit 1
fi

# Configurable settings.
QEMU_BIN="${QEMU_BIN:-qemu-system-i386}"
QEMU_SMP="${QEMU_SMP:-4}"
MACOS_QEMU_WIDTH="${MACOS_QEMU_WIDTH:-800}"
MACOS_QEMU_HEIGHT="${MACOS_QEMU_HEIGHT:-600}"
MACOS_QEMU_X="${MACOS_QEMU_X:-80}"
MACOS_QEMU_Y="${MACOS_QEMU_Y:-60}"

declare -a QEMU_ARGS_DISPLAY
if [[ $OSTYPE == darwin* ]]; then
    # Make the window resizable.
    QEMU_ARGS_DISPLAY+=(-display "${QEMU_DISPLAY:-cocoa,zoom-to-fit=on}")

    # Automatically focus the window and resize it.
    ( sleep 0.5
      qemu_pid="$(pgrep -n -x "$QEMU_BIN" || true)"
      [[ -n $qemu_pid ]] || return 0
      osascript >/dev/null 2>&1 <<EOF || true
tell application "System Events"
    tell first application process whose unix id is $qemu_pid
        set frontmost to true
        tell first window
            set position to {$MACOS_QEMU_X, $MACOS_QEMU_Y}
            set size to {$MACOS_QEMU_WIDTH, $MACOS_QEMU_HEIGHT}
        end tell
    end tell
end tell
EOF
    ) &
fi

declare -a QEMU_ARGS_SMP
QEMU_ARGS_SMP+=(-smp $QEMU_SMP)

for arg in "$@"; do
    case "$arg" in
        -smp)
            QEMU_ARGS_SMP=()
            ;;
        -display)
            QEMU_ARGS_DISPLAY=()
            ;;
    esac
done

"$QEMU_BIN" -cdrom "$ISO_PATH"                                \
            -drive file="$HD_PATH",format=raw,if=none,id=disk \
            -device ahci,id=ahci                              \
            -device ide-hd,drive=disk,bus=ahci.0              \
            -boot order=d                                     \
            -serial stdio                                     \
            -d guest_errors                                   \
            "${QEMU_ARGS_SMP[@]}"                             \
            "${QEMU_ARGS_DISPLAY[@]}"                         \
            "$@"
