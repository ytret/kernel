---
name: kernel-build
description: Configure, build, and test the x86 (32-bit) hobby kernel using CMake and Nix
---

## Configure the build

Creates `build/` if needed and runs CMake with the i686-elf toolchain:

```
mkdir -p build && cd build && nix develop .. --command cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -GNinja
```

## Build the kernel

Compiles the kernel and generates Doxygen docs:

```
cd build && nix develop .. --command ninja
```

## Create a bootable ISO

Packages the kernel into a bootable ISO image:

```
cd build && nix develop .. --command bash ./create_iso.sh
```

## Build and run host tests

Host tests live under `tests/` and are built separately (not in `build/`):

```
mkdir -p tests/build && cd tests/build && cmake .. -DCMAKE_BUILD_TYPE=Debug -GNinja && ninja
cd tests/build && ctest
```

## Build and run kernel tests

Kernel tests run inside QEMU. Create a test ISO first, then start the runner:

```
cd build && nix develop .. --command bash ./create_iso.sh -c "ktest.run-all-suites ktest.exit-vm" -o kernel-test.iso
cd build && ./ktest.sh
```

## Notes

- All commands expect the **project root** as the working directory.
- `i686-elf-*` cross-compiler tools are only available inside the `nix develop` shell, so any command that uses them must be wrapped with `nix develop .. --command <cmd>`.
- `cd` commands in the examples must be run in the same shell (use `&&` chaining or the equivalent in your execution environment).
