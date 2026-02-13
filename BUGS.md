# Bugs in src/system/kernel

This file lists potential bugs found in the kernel source code, categorized by severity and type.

## Critical / High Priority

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/vm/vm.cpp` | 2606 | **Security / Correctness** | TODO: "B_FULL_LOCK means that all pages are locked. We are not...". The implementation might not be fully respecting `B_FULL_LOCK` semantics, potentially leading to page faults in critical sections. |
| `src/system/kernel/timer.cpp` | 418 | **Logic Error** | FIXME: "Theoretically we should be able to skip this if (previous == NULL). But it seems adding that causes problems on some systems, possibly due to some other bug." |
| `src/system/kernel/debug/debug.cpp` | 662 | **Code Smell / Hack** | "HACK ALERT!!! If we get a $ at the beginning of the line...". Hardcoded parser hack in debug output. |

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
| `src/system/kernel/heap.cpp` | 1056 | **Manual Locking** | `heap_validate_heap` manually locks all bins in a loop. |
| `src/system/kernel/image.cpp` | 99, 138, 212, 239 | **Manual Locking** | Various functions in `image.cpp` use manual locking. |
| `src/system/kernel/locks/lock.cpp` | 15, 105 | **Manual Locking** | Manual `mutex_lock` usage. |
| `src/system/kernel/port.cpp` | 639 | **Manual Locking** | `create_port` uses `mutex_lock` manually (via `new Port`). |
| `src/system/kernel/condition_variable.cpp` | 316 | **Manual Locking** | `ConditionVariable::NotifyAll` uses manual lock/unlock. |
| `src/system/kernel/device_manager/legacy_drivers.cpp` | 523, 596, 606 | **Code Smell** | TODOs indicating temporary or suboptimal logic in driver loading. |
| `src/system/kernel/device_manager/legacy_drivers.cpp` | 614, 1077, 1114 | **Missing Feature** | Missing hot-reloading features and directory creation. |
| `src/system/kernel/device_manager/IOCache.cpp` | 323, 343 | **Optimization** | Optimization opportunities for read requests and low memory handling. |
| `src/system/kernel/device_manager/IOCache.cpp` | 675 | **Testing** | TODO: "_MapPages() cannot fail, so the fallback is never needed. Test which". |
| `src/system/kernel/fs/vfs.cpp` | 1818, 1839 | **Concurrency** | TODOs regarding deadlock detection and lock joining. |
| `src/system/kernel/port.cpp` | 692, 700 | **Resource Management** | TODOs for team limits and wait logic. |
| `src/system/kernel/sem.cpp` | 756 | **Cleanup** | "TODO: the B_CHECK_PERMISSION flag should be made private". |
| `src/system/kernel/elf.cpp` | 678 | **Logic** | "TODO: Revise the default version case!". |
| `src/system/kernel/thread.cpp` | 3978 | **Code Organization** | "TODO: the following two functions don't belong here". |
| `src/system/kernel/cpu.cpp` | 194 | **Missing Feature** | "TODO: data cache". |
| `src/system/kernel/arch/ppc/paging/460/PPCVMTranslationMap460.cpp` | 1066 | **Race Condition** | TODO: "Obvious race condition: Between querying and unmapping the". |
| `src/system/kernel/arch/arm/arch_system_info.cpp` | 43 | **Missing Feature** | TODO: "node->data.core.default_frequency = sCPUClockFrequency;". |
