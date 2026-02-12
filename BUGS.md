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
| `src/system/kernel/vm/vm_page.cpp` | 3489 | **Unchecked Allocation** | `create_area` return value is unchecked in `vm_page_init`. If area creation fails, the system might proceed in an inconsistent state or crash later. |
| `src/system/kernel/debug/guarded_heap.cpp` | 145 | **Unchecked Allocation** | `create_area` return value is unchecked in `guarded_heap_init`. Failure leads to uninitialized heap structures. |
| `src/system/kernel/vm/vm.cpp` | 2606 | **Security / Correctness** | TODO: "B_FULL_LOCK means that all pages are locked. We are not...". The implementation might not be fully respecting `B_FULL_LOCK` semantics, potentially leading to page faults in critical sections. |
| `src/system/kernel/device_manager/dma_resources.cpp` | 456 | **Logic Error** | `// TODO: !partialOperation || totalLength >= fBlockSize`. Potential logic error in DMA resource handling for partial operations. |
| `src/system/kernel/arch/generic/generic_vm_physical_page_mapper.cpp` | 111 | **Unchecked Lock** | `acquire_sem` return value ignored. If acquisition fails (e.g., interrupted), the code proceeds as if it holds the lock, violating mutual exclusion. |
| `src/system/kernel/arch/generic/generic_vm_physical_page_mapper.cpp` | 295, 300 | **Unchecked Allocation** | `create_area` return value ignored. Failure to create mapping areas is not handled. |
| `src/system/kernel/elf.cpp` | 2293 | **Integer Overflow** | `malloc(sizeof(elf_sym) * symbolCount)`. `symbolCount` comes from `num_debug_symbols` which might be controllable. Integer overflow could lead to small allocation and heap overflow. |
| `src/system/kernel/heap.cpp` | 2499 | **Integer Overflow** | `malloc(newSize)` where `newSize` is calculated. Overflow check exists in `realloc` but might be insufficient depending on `newSize` derivation. |
| `src/system/kernel/UserTimer.cpp` | 1078 | **Race Condition** | TODO: "To avoid odd race conditions, we should check the current time of...". Implies existing race condition in timer handling. |

## Medium Priority

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/fs/vfs.cpp` | 1208 | **Missing Feature** | `acquire_advisory_lock` has a TODO: "deadlock detection is complex and currently deferred". This means local file locking deadlocks are possible and undetected. |
| `src/system/kernel/fs/vfs.cpp` | 1992 | **Missing Feature** | `disconnect_mount_or_vnode_fds`: "TODO: there is currently no means to stop a blocking read/write!". Unmounting a busy FS might hang or leave threads blocked indefinitely. |
| `src/system/kernel/sem.cpp` | 1066 | **Unreliable Behavior** | `_get_next_sem_info` uses index-based iteration on a dynamic list (`sem_list`). If a semaphore is deleted from the middle of the list, iteration might skip entries. |
| `src/system/kernel/thread.cpp` | 1719 | **Concurrency Issue** | `drop_into_debugger` calls `_user_debug_thread` with a comment: "TODO: This is a non-trivial syscall doing some locking, so this is really nasty and may go seriously wrong." |
| `src/system/kernel/thread.cpp` | 305, 478, 483, 488, 932, 1028 | **Truncation** | `snprintf` return values are ignored. If the name is longer than the buffer (e.g. `B_OS_NAME_LENGTH`), it will be silently truncated, potentially causing confusion in debug tools. |
| `src/system/kernel/fs/vfs.cpp` | 1176 | **Unchecked Return** | `get_vnode` return value ignored. If `get_vnode` fails, `vnode` remains uninitialized or NULL, leading to crash. |
| `src/system/kernel/fs/vfs.cpp` | 3897 | **Unchecked Return** | `get_vnode` return value ignored. |
| `src/system/kernel/fs/vfs.cpp` | 4341 | **Unchecked Return** | `vfs_get_vnode` return value ignored. |
| `src/system/kernel/fs/vfs.cpp` | 4402 | **Unchecked Return** | `vfs_lookup_vnode` return value ignored. |
| `src/system/kernel/fs/vfs.cpp` | 6257 | **Unchecked Return** | `fix_dirent` return value ignored. If `fix_dirent` fails, the buffer might contain invalid data returned to user space. |
| `src/system/kernel/fs/fd.cpp` | 304 | **Unchecked Return** | `get_fd` return value ignored. |
| `src/system/kernel/cache/block_cache.cpp` | 812 | **Logic Error** | TODO: "this only works if the link is the first entry of block_cache". Dependency on struct layout is fragile. |
| `src/system/kernel/vm/vm.cpp` | 6709 | **Logic Error** | TODO: "fork() should automatically unlock memory in the child.". Missing feature that might lead to memory accounting issues or unexpected behavior in children. |
| `src/system/kernel/slab/MemoryManager.cpp` | 557 | **Missing Feature** | TODO: "Support CACHE_UNLOCKED_PAGES!". |
| `src/system/kernel/debug/tracing.cpp` | 1030 | **Unchecked Return** | `user_strlcpy` return value ignored. If it fails (e.g. invalid user address), the buffer might be uninitialized. |
| `src/system/kernel/debug/user_debugger.cpp` | 290, 2880, 3039 | **Unchecked Return** | `wait_for_thread` return value ignored. |
| `src/system/kernel/disk_device_manager/KDiskDeviceManager.cpp` | 294 | **Unchecked Return** | `wait_for_thread` return value ignored. |
| `src/system/kernel/disk_device_manager/KDiskDeviceManager.cpp` | 674 | **Locking Issue** | TODO: "This won't do. Locking the DDM while scanning the partition is not a...". Performance/Concurrency issue. |
| `src/system/kernel/disk_device_manager/KPartition.cpp` | 1631 | **Error Handling** | TODO: "handle this gracefully". Indicates unhandled error condition. |
| `src/system/kernel/team.cpp` | 2026 | **Resource Leak** | TODO: "remove team resources if there are any left". Indicates potential resource leak on team destruction. |
| `src/system/kernel/team.cpp` | 2684 | **Unchecked Return** | `wait_for_thread` return value ignored. |

## Low Priority / Code Quality / Potential Issues

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/elf.cpp` | 1628 | **Manual Locking** | `get_image_symbol` uses manual `mutex_lock` / `mutex_unlock` with `goto done`. Risk of missing unlock if return added early. Should use `MutexLocker`. |
| `src/system/kernel/elf.cpp` | 2284 | **Manual Locking** | `elf_add_memory_image_symbol` uses manual `mutex_lock`. |
| `src/system/kernel/heap.cpp` | 1056 | **Manual Locking** | `heap_validate_heap` manually locks all bins in a loop. |
| `src/system/kernel/image.cpp` | 99, 138, 212, 239 | **Manual Locking** | Various functions in `image.cpp` use manual locking. |
| `src/system/kernel/locks/lock.cpp` | 15, 105 | **Manual Locking** | Manual `mutex_lock` usage. |
| `src/system/kernel/thread.cpp` | 2851 | **Ignored Return** | `snprintf` return value ignored. |
| `src/system/kernel/thread.cpp` | 2865 | **Ignored Return** | `snprintf` return value ignored. |
| `src/system/kernel/port.cpp` | 639 | **Manual Locking** | `create_port` uses `mutex_lock` manually (via `new Port`). |
| `src/system/kernel/condition_variable.cpp` | 316 | **Manual Locking** | `ConditionVariable::NotifyAll` uses manual lock/unlock. |
| `src/system/kernel/device_manager/legacy_drivers.cpp` | 523 | **Code Smell** | TODO: "would it be better to initialize a static structure here". |
| `src/system/kernel/device_manager/legacy_drivers.cpp` | 596 | **Code Smell** | TODO: "do properly, but for now we just update the path if it". |
| `src/system/kernel/device_manager/legacy_drivers.cpp` | 606 | **Code Smell** | TODO: "check if this driver is a different one and has precedence". |
| `src/system/kernel/device_manager/legacy_drivers.cpp` | 614 | **Missing Feature** | TODO: "test for changes here and/or via node monitoring and reload". |
| `src/system/kernel/device_manager/legacy_drivers.cpp` | 1077 | **Missing Feature** | TODO: "adjust driver priority if necessary". |
| `src/system/kernel/device_manager/legacy_drivers.cpp` | 1114 | **Missing Feature** | TODO: "create missing directories?". |
| `src/system/kernel/device_manager/IOCache.cpp` | 323 | **Optimization** | TODO: "If this is a read request and the missing pages range doesn't intersect". |
| `src/system/kernel/device_manager/IOCache.cpp` | 343 | **Memory Management** | TODO: "When memory is low, we should consider cannibalizing ourselves". |
| `src/system/kernel/device_manager/IOCache.cpp` | 675 | **Testing** | TODO: "_MapPages() cannot fail, so the fallback is never needed. Test which". |
| `src/system/kernel/fs/vfs.cpp` | 1818 | **Concurrency** | `acquire_advisory_lock`: "TODO: deadlock detection is complex and currently deferred." |
| `src/system/kernel/fs/vfs.cpp` | 1839 | **Concurrency** | "TODO: locks from the same team might be joinable!". |
| `src/system/kernel/port.cpp` | 692 | **Resource Management** | "TODO: add per team limit". |
| `src/system/kernel/port.cpp` | 700 | **Logic Error** | "TODO: we don't want to wait - but does that also mean we". |
| `src/system/kernel/syscalls.cpp` | 285 | **Cleanup** | "TODO: we should only remove the syscall with the matching version". |
| `src/system/kernel/sem.cpp` | 756 | **Cleanup** | "TODO: the B_CHECK_PERMISSION flag should be made private". |
| `src/system/kernel/elf.cpp` | 678 | **Logic** | "TODO: Revise the default version case!". |
| `src/system/kernel/thread.cpp` | 3978 | **Code Organization** | "TODO: the following two functions don't belong here". |
| `src/system/kernel/cpu.cpp` | 194 | **Missing Feature** | "TODO: data cache". |
| `src/system/kernel/thread.cpp` | Many | **Unchecked Copy** | Multiple `user_memcpy` calls where return value checks rely on complex logic or might be missing in specific paths (scan required 100% verification, flagged as potential). |
| `src/system/kernel/elf.cpp` | 1360 | **Unchecked Malloc** | `malloc` for `debug_symbols` checked, but failure silently drops debug info, which might be unexpected behavior for the caller. |
| `src/system/kernel/DPC.cpp` | 156 | **Unchecked Return** | `wait_for_thread` return value ignored. |
| `src/system/kernel/sem.cpp` | 690, 704 | **Unchecked Return** | `create_sem` and `acquire_sem` return values ignored. |
| `src/system/kernel/lib/kernel_vsprintf.cpp` | 502 | **Buffer Overflow Risk** | Unsafe usage of `sprintf`. Use `snprintf` instead. |
| `src/system/kernel/util/queue.cpp` | 116 | **Integer Overflow** | `malloc(size * sizeof(void *))` in `fixed_queue_init`. `size` is `int`, multiplication can overflow. |
| `src/system/kernel/util/RadixBitmap.cpp` | 185 | **Integer Overflow** | `malloc(bmp->root_size * sizeof(radix_node))`. `root_size` is calculated from user input (indirectly). |
| `src/system/kernel/cache/block_cache.cpp` | 1174 | **Integer Overflow** | `malloc(newCapacity * sizeof(void*))`. `newCapacity` calculation could overflow. |
| `src/system/kernel/cache/file_map.cpp` | 43 | **Memory Optimization** | TODO: "use a sparse array - eventually, the unused BlockMap would be something". |
| `src/system/kernel/vm/vm.cpp` | 5121 | **Unchecked Return** | `user_strlcpy` return value ignored. |
| `src/system/kernel/arch/ppc/paging/460/PPCVMTranslationMap460.cpp` | 1066 | **Race Condition** | TODO: "Obvious race condition: Between querying and unmapping the". |
| `src/system/kernel/arch/arm/arch_system_info.cpp` | 43 | **Missing Feature** | TODO: "node->data.core.default_frequency = sCPUClockFrequency;". |
| `src/system/kernel/disk_device_manager/disk_device_manager.cpp` | 345 | **Unchecked Return** | `acquire_sem` return value ignored. |
