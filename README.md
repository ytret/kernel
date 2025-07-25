# ytret's kernel

## Kernel features

* Text-mode or framebuffer terminal
* Kernel-mode shell
* Device manager with Convenital PCI enumeration
* Symmetric Multiprocessing (SMP) support
* Per-processor round-robin task scheduler
* Task synchronization primitives: spinlocks, mutexes, semaphores, FIFO queues
* Asynchronous block device layer
* AHCI driver with read/write support
* GUID Partition Table parsing
* Bare-bones syscall support for usermode tasks


## Building and running

### Requirements

Arch Linux packages are specified in parentheses.

1. Building:

  * binutils and GCC targeting i686-elf
  * CMake (`cmake`)
  * Optionally, Ninja (`ninja`)

2. Creating a bootable ISO:

  * grub-mkrescue or grub2-mkrescue (`grub`)
  * xorriso (`libisoburn`)
  * GNU mtools (`mtools`)

3. Creating a virtual disk:

  * QEMU disk image manipulation tools (`qemu-img`)
  * mkfs.ext4 (`e2fsprogs`)

4. Running in an emulator:

  * QEMU desktop for x86 (`qemu-desktop`, `qemu-system-x86`)

### Building

```
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
$ make  # or ninja
```

Optionally, pass `-GNinja` to `cmake` to use Ninja instead of GNU make.

### Running

1. Create an ISO with GRUB and the kernel:

    ```
    $ ./create_iso.sh
    ```

2. Create and partition a virtual disk image:

    ```
    $ qemu-img create hd.img 4M
    $ fdisk hd.img
    > g  # new GUID partition table (GPT)
    > n  # new partition, accept the default values
    > t  # change the partition type
    >> F0516EBC-2D9E-4206-ABFC-B14EC7A626CE
    > w  # write changes and exit
    $ sudo losetup -P loop0 hd.img
    $ sudo mkfs.ext4 /dev/loop0p1
    $ sudo losetup -d loop0
    ```

3. Run QEMU:

    ```
    $ ./run_qemu.sh
    ```
