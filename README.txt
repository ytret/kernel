KERNEL FEATURES
===============

* Text-mode or framebuffer terminal
* PCI device enumeration
* Reading from an AHCI disk with a GPT disklabel
* Preemptive multitasking
* Usermode programs with syscalls


BUILDING AND RUNNING
====================

Requirements
------------

Arch Linux packages are specified in parentheses.

1. Building:

  * binutils and GCC cross-compiled for i686-elf
  * CMake (cmake)
  * Optionally, Ninja (ninja)

2. Creating an ISO:

  * grub-mkrescue or grub2-mkrescue (grub)
  * xorriso (libisoburn)
  * GNU mtools (mtools)

3. Creating a virtual disk:

  * QEMU disk image manipulating tools (qemu-img)
  * mkfs.ext4 (e2fsprogs)

3. Running in an emulator:

  * QEMU for x86 (qemu-desktop, qemu-system-x86)


Building
--------

$ mkdir build
$ cd build
$ cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DCMAKE_BUILD_TYPE=Debug

Optionally, pass `-GNinja' to `cmake' to use Ninja instead of GNU make.


Running
-------

1. Create an ISO:

  $ ./create_iso.sh

2. Create and partition a virtual disk image:

  $ qemu-img create hd.img 4M
  $ fdisk hd.img
  > g  # new GUID partition table (GPT)
  > n  # new partition, accept the default values
  > t  # change the partition type
  >> F0516EBC-2D9E-4206-ABFC-B14EC7A626CE
  > w  # write changes and exit
  $ sudo losetup -P loop0 hd.img
  $ sudo mkfs.ext4 /dev/loop0p1
  $ sudo losetup -D loop0

3. Run QEMU:

  $ ./run_qemu.sh
