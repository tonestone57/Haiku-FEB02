# Bugs in src/system/kernel

This file lists potential bugs found in the kernel source code, categorized by severity and type.

## Critical / High Priority

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/fs/vfs.cpp` | 748, 2106 | **Deadlock** | `fs_unmount` holds `sMountLock` (Read) and waits for `sVnodeLock` (Write). `create_new_vnode_and_lock` (called by `get_vnode`) holds `sVnodeLock` (Write) and waits for `sMountLock` (Read). If a writer is pending on `sMountLock` (blocking readers), this causes a deadlock cycle. |
| `src/system/kernel/fs/vfs.cpp` | 2156 | **Live Lock** | `fs_unmount` busy-waits in a loop with `snooze(100000)` if vnodes are still referenced. This can lead to a live lock if the reference count never drops. Documented in TODO comment (line 2200). |
| `src/system/kernel/port.cpp` | 713 | **Race Condition / Lost Wakeup** | In `get_port_message`, `sWaitingForSpace` is incremented *after* checking `sNoSpaceCondition`. A writer might check `sWaitingForSpace` (seeing 0) and skip `NotifyAll` before the reader increments it and waits, causing the reader to sleep forever. |
| `src/system/kernel/fs/vfs.cpp` | 1279 | **Panic Risk** | `lookup_dir_entry` panics if `FS_CALL(lookup)` returns success but the vnode wasn't registered/found. This allows a buggy file system to crash the kernel via `panic`. |
| `src/system/kernel/device_manager/IORequest.cpp` | 178, 198 | **VIP Violation / Priority Inversion** | `vm_map_physical_memory_vecs` and `vm_get_physical_page` are called without VIP flags even if the IORequest is VIP. This violates the VIP requirement and can lead to deadlocks during swap operations. |
| `src/system/kernel/vm/vm.cpp` | 2606 | **Security / Correctness** | TODO: "B_FULL_LOCK means that all pages are locked. We are not...". The implementation might not be fully respecting `B_FULL_LOCK` semantics, potentially leading to page faults in critical sections. |
| `src/system/kernel/device_manager/dma_resources.cpp` | 456 | **Logic Error** | `// TODO: !partialOperation || totalLength >= fBlockSize`. Potential logic error in DMA resource handling for partial operations. |
| `src/system/kernel/arch/generic/generic_vm_physical_page_mapper.cpp` | 111 | **Unchecked Lock** | `acquire_sem` return value ignored. If acquisition fails (e.g., interrupted), the code proceeds as if it holds the lock, violating mutual exclusion. |
| `src/system/kernel/UserTimer.cpp` | 1078 | **Race Condition** | TODO: "To avoid odd race conditions, we should check the current time of...". Implies existing race condition in timer handling. |
| `src/system/kernel/cache/block_cache.cpp` | 3715, 3719 | **Unchecked Lock** | `mutex_lock` and `rw_lock_write_lock` return values ignored in `block_cache_delete`. Failure here (though unlikely for mutex) would proceed with a deleted lock. |
| `src/system/kernel/disk_device_manager/disk_device_manager.cpp` | 345 | **Unchecked Lock** | `acquire_sem` return value ignored. |
| `src/system/kernel/timer.cpp` | 418 | **Logic Error** | FIXME: "Theoretically we should be able to skip this if (previous == NULL). But it seems adding that causes problems on some systems, possibly due to some other bug." |
| `src/system/kernel/debug/debug.cpp` | 662 | **Code Smell / Hack** | "HACK ALERT!!! If we get a $ at the beginning of the line...". Hardcoded parser hack in debug output. |

## Medium Priority

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/fs/vfs.cpp` | 1208 | **Missing Feature** | `acquire_advisory_lock` has a TODO: "deadlock detection is complex and currently deferred". This means local file locking deadlocks are possible and undetected. |
| `src/system/kernel/fs/vfs.cpp` | 1992 | **Missing Feature** | `disconnect_mount_or_vnode_fds`: "TODO: there is currently no means to stop a blocking read/write!". Unmounting a busy FS might hang or leave threads blocked indefinitely. |
| `src/system/kernel/sem.cpp` | 1066 | **Unreliable Behavior** | `_get_next_sem_info` uses index-based iteration on a dynamic list (`sem_list`). If a semaphore is deleted from the middle of the list, iteration might skip entries. |
| `src/system/kernel/thread.cpp` | 1719 | **Concurrency Issue** | `drop_into_debugger` calls `_user_debug_thread` with a comment: "TODO: This is a non-trivial syscall doing some locking, so this is really nasty and may go seriously wrong." |
| `src/system/kernel/fs/vfs.cpp` | 1176, 3897, 4341, 4402 | **Unchecked Return** | `get_vnode` / `vfs_get_vnode` return values ignored in some paths. If `get_vnode` fails, the vnode pointer remains uninitialized or NULL, possibly leading to crash. |
| `src/system/kernel/fs/vfs.cpp` | 6257 | **Unchecked Return** | `fix_dirent` return value ignored. If `fix_dirent` fails (e.g., resolving covered vnodes), the buffer might contain invalid data returned to user space. |
| `src/system/kernel/fs/fd.cpp` | 304 | **Unchecked Return** | `get_fd` return value ignored. |
| `src/system/kernel/cache/block_cache.cpp` | 812 | **Logic Error** | TODO: "this only works if the link is the first entry of block_cache". Dependency on struct layout is fragile. |
| `src/system/kernel/vm/vm.cpp` | 6709 | **Logic Error** | TODO: "fork() should automatically unlock memory in the child.". Missing feature that might lead to memory accounting issues or unexpected behavior in children. |
| `src/system/kernel/slab/MemoryManager.cpp` | 557 | **Missing Feature** | TODO: "Support CACHE_UNLOCKED_PAGES!". |
| `src/system/kernel/disk_device_manager/KDiskDeviceManager.cpp` | 674 | **Locking Issue** | TODO: "This won't do. Locking the DDM while scanning the partition is not a...". Performance/Concurrency issue. |
| `src/system/kernel/disk_device_manager/KPartition.cpp` | 1631 | **Error Handling** | TODO: "handle this gracefully". Indicates unhandled error condition. |
| `src/system/kernel/team.cpp` | 2026 | **Resource Leak** | TODO: "remove team resources if there are any left". Indicates potential resource leak on team destruction. |
| `src/system/kernel/team.cpp` | 765 | **Assignment in Condition** | `if (const char* lastSlash = strrchr(name, '/'))`. Valid pattern but often flagged. |
| `src/system/kernel/thread.cpp` | 2519 | **Assignment in Condition** | `if (UserTimer* timer = thread->UserTimerFor(USER_TIMER_REAL_TIME_ID))`. Valid C++ pattern but increases cognitive load and risk of typo (= vs ==). |
| `src/system/kernel/fs/vfs.cpp` | 5637 | **Unchecked Lock** | `rw_lock_read_lock` ignored in `common_fcntl`. |

## Low Priority / Code Quality / Potential Issues

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/elf.cpp` | 1628, 2284 | **Manual Locking** | `get_image_symbol` and others use manual `mutex_lock` / `mutex_unlock` with `goto done`. Risk of missing unlock if return added early. Should use `MutexLocker`. |
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
| `src/system/kernel/syscalls.cpp` | 285 | **Cleanup** | "TODO: we should only remove the syscall with the matching version". |
| `src/system/kernel/sem.cpp` | 756 | **Cleanup** | "TODO: the B_CHECK_PERMISSION flag should be made private". |
| `src/system/kernel/elf.cpp` | 678 | **Logic** | "TODO: Revise the default version case!". |
| `src/system/kernel/thread.cpp` | 3978 | **Code Organization** | "TODO: the following two functions don't belong here". |
| `src/system/kernel/cpu.cpp` | 194 | **Missing Feature** | "TODO: data cache". |
| `src/system/kernel/arch/ppc/paging/460/PPCVMTranslationMap460.cpp` | 1066 | **Race Condition** | TODO: "Obvious race condition: Between querying and unmapping the". |
| `src/system/kernel/arch/arm/arch_system_info.cpp` | 43 | **Missing Feature** | TODO: "node->data.core.default_frequency = sCPUClockFrequency;". |
