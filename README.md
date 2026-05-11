# ytret's kernel

An experimental x86 kernel written in C.

This README covers the fastest path to build, run, debug, and test the kernel
locally.

## Highlights

* Text-mode and framebuffer terminals
* Kernel shell with a live Lua REPL
* Device manager with Conventional PCI enumeration
* Per-processor round-robin scheduler with SMP support
* Asynchronous block device layer for disks and GPT partitions
* AHCI driver with read and write support
* Basic syscall support for user-mode tasks
* In-kernel tests for complex subsystems

## Build and run

### Requirements

You can either:

1. Use [`nix`](https://nixos.org/download/#download-nix) to enter a ready-made
   development environment.
2. Install the required packages manually.

If you use `nix`, enter a development shell with one of the following commands:

```bash
nix develop .           # enter the default devShell from flake.nix
nix develop '.#minimal' # enter the minimal devShell from flake.nix
nix develop . -c fish   # use fish instead of bash
nix develop -i .        # clean environment; QEMU will not be available
```

If you install dependencies manually, you will need:

1. For building:
   * `binutils` and `gcc` targeting `i686-elf`
   * `cmake`
   * Optional: `ninja`
   * Optional: `doxygen`
2. For creating a bootable ISO:
   * `xorriso`
3. For creating a virtual disk image:
   * `qemu-img`
   * `mkfs.ext4`
4. For running the kernel in an emulator:
   * `qemu-system-i386` or an equivalent QEMU x86 system package

Arch Linux and Homebrew package names may differ slightly from the generic names
above.

### Building

Configure and build from a separate build directory:

```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DCMAKE_BUILD_TYPE=Debug
make
```

If you prefer Ninja:

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DCMAKE_BUILD_TYPE=Debug -GNinja
ninja
```

### Running

1. Create a bootable ISO with Limine and the kernel:

   ```bash
   ./create_iso.sh
   ```

2. Create an empty virtual disk image from the repository root:

   ```bash
   qemu-img create hd.img 4M
   ```

3. Optionally partition the disk image and create a filesystem:

   ```bash
   fdisk hd.img
   # g  -> create a new GPT
   # n  -> create a new partition and accept the defaults
   # t  -> change the partition type
   # F0516EBC-2D9E-4206-ABFC-B14EC7A626CE
   # w  -> write changes and exit

   ./tools/loop-hd-image.sh
   sudo mkfs.ext4 /dev/loop0p1
   sudo losetup -d /dev/loop0
   ```

4. Start QEMU from the build directory:

   ```bash
   ./run_qemu.sh
   ```

The `create_iso.sh` and `run_qemu.sh` helper scripts are generated in the build
directory by CMake.

### Debugging

You can debug the kernel with GDB while it runs inside QEMU.

1. Start QEMU with GDB support enabled:

   ```bash
   ./run_qemu.sh -S -s
   ```

   `-S` starts the VM in a paused state, and `-s` opens a GDB server on port
   `1234`.

2. In the build directory, start GDB:

   ```bash
   gdb kernel
   ```

3. Once GDB connects, continue execution:

   ```gdb
   continue
   ```

CMake places a `.gdbinit` file in the build directory that automatically
connects GDB to QEMU on port `1234`.

## Testing

This project has two separate test layers.

### Host tests

Host tests run on the build machine through CMake and CTest. They are intended
for components that can be tested outside the live kernel.

```bash
mkdir tests/build
cd tests/build
cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja
ninja
ctest --output-on-failure
```

### Kernel tests

Kernel tests run inside QEMU and cover subsystems that depend on a real kernel
environment or tighter integration across components.

```bash
# Run from the main build directory after the initial CMake configure step.
cmake .. -DYTKERNEL_ENABLE_TESTS=ON
ninja
./create_iso.sh -c "ktest.run-all-suites ktest.exit-vm" -o kernel-ktest.iso
./ktest.sh
```

`ktest.sh` starts QEMU in a test-oriented configuration, redirects kernel logs
to a file, and prints TAP-style results to stdout:

```text
TAP version 14
ok 1 - SMPSuiteMutex.Contention
ok 2 - SMPSuiteMutex.Handoff
ok 3 - SMPSuiteVNode.RefCount
ok 4 - SMPSuiteVNode.ConcurrentResolve
```
