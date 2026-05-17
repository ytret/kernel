---
name: check-build-type
description: Determine whether the kernel is configured for a Debug or Release build by querying CMake cache
---

Run from the project root:

```
cd build && cmake .. -L | grep CMAKE_BUILD_TYPE
```

This prints the current build type (e.g. `Debug` or `Release`). If `build/` does not exist or has not been configured, the command will fail — configure first using the `kernel-build` skill.
