#!/usr/bin/env bash
#
# Creates a raw disk image for testing AHCI.
#
# It is written word-by-word, with the higher byte of each word being the sector
# number plus higher byte of the word's byte index in the sector, and the lower
# byte being the lower byte of the word's byte index.  For example:
#
#   sector 0, word   0 -> 00 00
#   sector 0, word   1 -> 00 02 -- word 1 begins at byte 2
#   ..
#   sector 0, word 127 -> 01 fe -- word 127 begins at byte 1fe
#   sector 1, word   0 -> 01 00 -- sector 1 in higher byte
#   sector 1, word   1 -> 01 02
#   ..
#   sector 1, word 127 -> 02 fe -- sector 1 + higher byte of 1fe (1) add up to 2
#
# This may be convoluted, but allows to test that AHCI can read several sectors
# at once.

set -euo pipefail

IMG_PATH=hd.img

if [[ -e $IMG_PATH ]]; then
    echo "File at '$IMG_PATH' already exists. Please delete it or change" \
         "IMG_PATH in '$0'."
    exit 1
fi

for sector in $(seq 0 63); do
    echo "Sector $sector"
    for word in $(seq 0 255); do
        byte=$((2 * $word))
        hi=$(((($byte >> 8) + $sector) & 0xFF))
        lo=$(($byte & 0xFF))
        fmt=$(printf "%s%02x%s%02x" "\x" $hi "\x" $lo)
        printf "$fmt" | dd of=$IMG_PATH bs=1 count=2 conv=notrunc \
                           oseek=$((512 * $sector + 2 * $word)) status=none
    done
done
