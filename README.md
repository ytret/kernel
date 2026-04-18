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

You can use [`nix`](https://nixos.org/download/#download-nix) to create a build
environment with all required packages, or you can install the packages
manually.

If you choose to use `nix`, run one of these commands to enter a shell, in
which you will be able to build the kernel:

```
nix develop .           # to enter the 'default' devShell from 'flake.nix'
nix develop '.#minimal' # to enter the 'minimal' devShell from 'flake.nix'
nix develop . -c fish   # to use fish instead of bash
nix develop -i .        # for a clean build environment (QEMU won't work)
```

If you choose to install the packages manually, the list is provided below. Arch Linux
package names are provided in parentheses.

1. Building:

  * binutils and GCC targeting i686-elf
  * CMake (`cmake`)
  * Optionally: Ninja (`ninja`)
  * Optionally: Doxygen (`doxygen`)

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

Whether you choose to use a nix devShell or install the packages yourself, run
these commands to build the kernel:

```
$ mkdir build
$ cd build
$ cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
$ make  # or ninja
```

Optionally, pass `-GNinja` to `cmake` to use Ninja instead of GNU Make.

### Running

1. Create an ISO containing GRUB and the kernel:

   ```
    $ ./create_iso.sh
   ```

2. Create an empty virtual disk image:

   ```
    $ qemu-img create hd.img 4M
   ```

3. Optionally, partition the disk and create a file system:

   ```
    $ fdisk hd.img
    > g  # new GUID partition table (GPT)
    > n  # new partition, accept the default values
    > t  # change the partition type
    >> F0516EBC-2D9E-4206-ABFC-B14EC7A626CE
    > w  # write changes and exit

    $ ./tools/loop-hd-image.sh
    $ sudo mkfs.ext4 /dev/loop0p1
    $ sudo losetup -d /dev/loop0
   ```

4. Run QEMU:

   ```
    $ ./run_qemu.sh
   ```

### Debugging

You can use GDB to debug the kernel running inside QEMU.

1. Run QEMU with the following flags:

   * `-S` to freeze the CPU at startup
   * `-s` to listen for GDB on port 1234

   ```
   $ ./run_qemu.sh -S -s
   ```

2. Run GDB in the build directory:

   ```
    $ cd build
    $ gdb kernel
    > continue  # to unfreeze the emulation
   ```

   CMake should have placed a special `.gdbinit` file in the build directory
   that automatically connects to QEMU on port 1234.
