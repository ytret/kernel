# Project Overview

Custom x86 (32-bit) hobby kernel written in C23 with GNU extensions.

Goals:

- Unix compatibility (Linux / BSD)
- SMP support

Non-goals:

- User-space compatibility for now

# Source Tree Structure

* `build/` - main CMake build directory
* `cmake/` - CMake modules (functions)
* `docs/` - Doxygen config template and generated HTML docs
* `gdb/` - GDB scripts
* `include/` - public kernel include directory
* `isodir/` - ISO overlay used to create a bootable ISO using `create_iso.sh`
* `klibc/` - a shim kernel-mode libc used by Lua
* `lua/` - Lua kernel-mode shell source code
* `src/` - kernel source code
* `tests/` - host tests
* `tools/` - host tools like `create_iso.sh` and `run_qemu.sh`
* `user/` - bare-bones user-mode program

# Code Style Guide

## Source Code Structure

A pair of source and header files is called *a module*. Do not confuse with
kernel modules.

Source files (`*.c`) have the following structure:

1. Includes
2. Private macro definitions
3. Private type declarations and definitions
4. Variables
    1. Private global variable definitions
    2. Public global variable definitions for variables owned by this module
    3. Public global variable declarations (with `extern`) for variables defined
       in the linker script
5. Functions
    1. Private (static) function declarations
    2. Public function definitions
    3. Private (static) function definitions

Header files (`*.h`) have the following structure:

1. Include guard (only `#pragma once` is allowed)
2. Includes
3. Public macro definitions
4. Public type declarations and definitions
5. Functions
    1. Inline private (static) function definitions
    2. Public function declarations
6. Public global variable declarations (with `extern`) for variables owned by
   this module

## Naming Conventions

All public functions and variables have the module name as their prefix.
Generally the module name is the same as the file name, but may be shorter
(e.g., `ksh_` instead of `kshell_`). Prefixes must be consistent. There can be
exceptions if they increase readability.

All private functions have a `prv_` prefix before the module prefix.

Global variables also have a `g_` prefix before the module prefix.

## `const`

Strive to use `const` wherever possible, to prevent accidental modifications of
variables. `static` objects that do not change, e.g. an ops struct, must also
be `const`.

Place `const` before the type, e.g. `const char *`, NOT `char const *`. Some of
the source code uses the wrong convention, do not follow it.

## `default` case in `switch` statements

Try to omit the `default` case, instead use a `bool` variable and an `ASSERT` to
check that the enum value is correct. Omitting the `default` case causes the
compiler to generate a warning that is visible to the developer, so adding a new
enum field makes all its use places pop up in the warnings.

If you *need* the `default` case, consider rewriting the code using `if`
statements. If that makes the code too complicated, a `default` case is allowed
to be used.

Example:

```
bool type_ok = false;
switch (some_enum) {
case ENUM_VAL_1:
    type_ok = true;
    // <...>
    break;
}
ASSERT(type_ok);
```

## Formatting

Use `clang-format` to format the files. All committed files are expected to be
already formatted using `clang-format`, so don't worry about introducing
accidental formatting changes.

Example command:

```
clang-format -i src/file.c
```

## Documentation

Use Doxygen to *optionally* document files, macros, types, functions, global
variables. Prepend Doxygen commands with `@`, not a backslash.

Separate different parts of a doc comment, e.g. `@param` and `@return`.

Use `@warning` for non-obvious stuff that will probably lead to hard-to-detect
bugs like invalid type casts or silent panics. Use `@note` for other important
stuff.

# Build

The build system is CMake. Building is done inside a nix shell. There should be
a build directory at `build`. The following steps should be done only inside
the build directory.

Configure command (run from the `build` directory):

```
mkdir -p build
cd build
nix develop .. --command cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -GNinja
```

Change build type to `Debug` or `Release` (after initial configuration, run from
the `build` directory):

```
cd build
nix develop .. --command cmake .. -DCMAKE_BUILD_TYPE=Debug
```

Build command (it also generates docs, run from the `build` directory):

```
cd build
nix develop .. --command ninja
```

Create a bootable ISO with no tests (run from the `build` directory):

```
cd build
nix develop .. --command bash ./create_iso.sh
```

Run the ISO in an interactive QEMU session (run from the `build` directory):

```
cd build
./run_qemu.sh
```

# Testing

There are two separate test layers.

## Host tests

Host tests are implemented as a separate project that pulls the kernel sources,
see `tests/CMakeLists.txt`. Do not confuse with `src/test/`.

Build host tests (note: *do NOT* build the host tests at `tests/`, build them
at `tests/build/`):

```
mkdir -p tests/build
cd tests/build
cmake .. -DCMAKE_BUILD_TYPE=Debug -GNinja
ninja
```

Run the host tests:

```
cd tests/build
ctest
```

Or you can run individual Google Test tests e.g. `./tests/build/dynarr_test`,
they accept Google Test arguments.

## Kernel tests

Kernel tests run inside a non-interactive QEMU session and cover subsystems that
depend on a real kernel environment.

Create a bootable ISO with live kernel tests (run from the `build` directory):

```
cd build
nix develop .. --command bash ./create_iso.sh -c "ktest.run-all-suites ktest.exit-vm" -o kernel-test.iso
```

Start a test runner (run from the `build` directory):

```
cd build
./ktest.sh
```

Example output:

```
TAP version 14
ok 1 - SMPSuiteMutex.Contention
ok 2 - SMPSuiteMutex.Handoff
ok 3 - SMPSuiteVNode.RefCount
ok 4 - SMPSuiteVNode.ConcurrentResolve
```

The `ktest.sh` can hang in case the kernel panics. In that case, you need to
kill the test runner with SIGTERM or SIGKILL - it will close QEMU
automatically.

# Warning about `cd`

You are probably running shell commands in a subshell, so commands like `cd`
and `pushd` affect only the current running command. Make sure to add a `cd`
command to enter the correct directory.

# Warning about `i686-elf-` tools

These tools are present only in the `nix` environment. Make sure you use the
`nix develop .. --command <command>` pattern specified above (`..` is the
relative path to the project root).

If build commands fail, you are definitely not running `nix develop` correctly
or running it in a wrong directory.
