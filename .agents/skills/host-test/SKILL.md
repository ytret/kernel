---
name: host-test
description: Build and run host-side unit tests (Google Test) for kernel data structures and utilities
---

Host tests are a standalone CMake project under `tests/` that compiles and runs kernel source files on the host machine.

## Build host tests

1. Use the `bash` tool to create the `tests/build` directory if it doesn't exist.
2. Configure the host tests project in `tests/build` using `cmake .. -DCMAKE_BUILD_TYPE=Debug -GNinja` or `cmake .. -DCMAKE_BUILD_TYPE=Release -GNinja`.
3. Build the host tests in `tests/build` using `ninja`.

## Run all host tests

1. Use the `bash` tool inside `tests/build` to run `ctest`.

## Run a specific test

1. Glob `*_test` files inside `tests/build` to find compiled host tests.
2. If the test executable exists, use the `bash` tool inside `tests/build` to run it as an executable file.
3. Example of individual test names: `dynarr_test`, `list_test`, `queue_test`, etc.
4. These test executables accept standard Google Test arguments, pass them `--gtest-help` to see the help message.
