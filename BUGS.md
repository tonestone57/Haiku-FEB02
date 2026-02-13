# Bugs in src/system/kernel

This file lists potential bugs found in the kernel source code, categorized by severity and type.

## Medium Priority

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/fs/vfs.cpp` | 1208 | **Missing Feature** | `acquire_advisory_lock` has a TODO: "deadlock detection is complex and currently deferred". This means local file locking deadlocks are possible and undetected. |
| `src/system/kernel/fs/vfs.cpp` | 1992 | **Missing Feature** | `disconnect_mount_or_vnode_fds`: "TODO: there is currently no means to stop a blocking read/write!". Unmounting a busy FS might hang or leave threads blocked indefinitely. |
| `src/system/kernel/thread.cpp` | 1719 | **Concurrency Issue** | `drop_into_debugger` calls `_user_debug_thread` with a comment: "TODO: This is a non-trivial syscall doing some locking, so this is really nasty and may go seriously wrong." |
| `src/system/kernel/vm/vm.cpp` | 6709 | **Logic Error** | TODO: "fork() should automatically unlock memory in the child.". Missing feature that might lead to memory accounting issues or unexpected behavior in children. |
| `src/system/kernel/slab/MemoryManager.cpp` | 557 | **Missing Feature** | TODO: "Support CACHE_UNLOCKED_PAGES!". |
| `src/system/kernel/disk_device_manager/KDiskDeviceManager.cpp` | 674 | **Locking Issue** | TODO: "This won't do. Locking the DDM while scanning the partition is not a...". Performance/Concurrency issue. |

## Low Priority / Code Quality / Potential Issues

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/port.cpp` | 639 | **Manual Locking** | `create_port` uses `mutex_lock` manually (via `new Port`). |
| `src/system/kernel/device_manager/legacy_drivers.cpp` | 614, 1077 | **Missing Feature** | Missing hot-reloading features. |
| `src/system/kernel/device_manager/IOCache.cpp` | 323, 343 | **Optimization** | Optimization opportunities for read requests and low memory handling. |
| `src/system/kernel/fs/vfs.cpp` | 1818, 1839 | **Concurrency** | TODOs regarding deadlock detection and lock joining. |
| `src/system/kernel/port.cpp` | 692, 700 | **Resource Management** | TODOs for team limits and wait logic. |
| `src/system/kernel/sem.cpp` | 756 | **Cleanup** | "TODO: the B_CHECK_PERMISSION flag should be made private". |
| `src/system/kernel/arch/ppc/paging/460/PPCVMTranslationMap460.cpp` | 1066 | **Race Condition** | TODO: "Obvious race condition: Between querying and unmapping the". |
