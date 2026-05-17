---
name: kernel-test
description: Build and run live in-QEMU kernel test suites (SMP, VNode, Mutex, etc.)
---

Kernel tests run inside a non-interactive QEMU session and cover subsystems that depend on a real kernel environment (SMP, virtual filesystem, etc.).

## Create a kernel test ISO

```
cd build && nix develop .. --command bash ./create_iso.sh -c "ktest.run-all-suites ktest.exit-vm" -o kernel-test.iso
```

## Start the test runner

```
cd .. && cd build && ./ktest.sh
```

## Troubleshooting

If `ktest.sh` hangs (e.g. after a kernel panic), kill it with SIGTERM/SIGKILL — it will close QEMU automatically.

## Expected output

```
TAP version 14
ok 1 - SMPSuiteMutex.Contention
ok 2 - SMPSuiteMutex.Handoff
ok 3 - SMPSuiteVNode.RefCount
ok 4 - SMPSuiteVNode.ConcurrentResolve
```
