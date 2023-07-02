#!/usr/bin/env bash

set -euxo pipefail

qemu-system-i386 -cdrom kernel.iso -d guest_errors "$@"
