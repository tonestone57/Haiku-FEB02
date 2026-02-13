# Bugs in src/system/kernel

This file lists potential bugs found in the kernel source code, categorized by severity and type.

## Medium Priority

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/fs/vfs.cpp` | 1208 | **Missing Feature** | `acquire_advisory_lock` has a TODO: "deadlock detection is complex and currently deferred". This means local file locking deadlocks are possible and undetected. |
| `src/system/kernel/fs/vfs.cpp` | 1992 | **Missing Feature** | `disconnect_mount_or_vnode_fds`: "TODO: there is currently no means to stop a blocking read/write!". Unmounting a busy FS might hang or leave threads blocked indefinitely. |
| `src/system/kernel/disk_device_manager/KDiskDeviceManager.cpp` | 674 | **Locking Issue** | TODO: "This won't do. Locking the DDM while scanning the partition is not a...". Performance/Concurrency issue. |

## Low Priority / Code Quality / Potential Issues

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/port.cpp` | 639 | **Manual Locking** | `create_port` uses `mutex_lock` manually (via `new Port`). |
| `src/system/kernel/device_manager/legacy_drivers.cpp` | 523, 596, 606 | **Code Smell** | TODOs indicating temporary or suboptimal logic in driver loading. |
| `src/system/kernel/device_manager/legacy_drivers.cpp` | 614, 1077 | **Missing Feature** | Missing hot-reloading features. |
| `src/system/kernel/device_manager/IOCache.cpp` | 323, 343 | **Optimization** | Optimization opportunities for read requests and low memory handling. |
| `src/system/kernel/fs/vfs.cpp` | 1818, 1839 | **Concurrency** | TODOs regarding deadlock detection and lock joining. |
| `src/system/kernel/port.cpp` | 692, 700 | **Resource Management** | TODOs for team limits and wait logic. |
| `src/system/kernel/sem.cpp` | 756 | **Cleanup** | "TODO: the B_CHECK_PERMISSION flag should be made private". |
| `src/system/kernel/arch/ppc/paging/460/PPCVMTranslationMap460.cpp` | 1066 | **Race Condition** | TODO: "Obvious race condition: Between querying and unmapping the". |
