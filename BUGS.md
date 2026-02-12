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

## Medium Priority

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/fs/vfs.cpp` | 1208 | **Missing Feature** | `acquire_advisory_lock` has a TODO: "deadlock detection is complex and currently deferred". This means local file locking deadlocks are possible and undetected. |
| `src/system/kernel/fs/vfs.cpp` | 1992 | **Missing Feature** | `disconnect_mount_or_vnode_fds`: "TODO: there is currently no means to stop a blocking read/write!". Unmounting a busy FS might hang or leave threads blocked indefinitely. |
| `src/system/kernel/sem.cpp` | 1066 | **Unreliable Behavior** | `_get_next_sem_info` uses index-based iteration on a dynamic list (`sem_list`). If a semaphore is deleted from the middle of the list, iteration might skip entries. |
| `src/system/kernel/thread.cpp` | 1719 | **Concurrency Issue** | `drop_into_debugger` calls `_user_debug_thread` with a comment: "TODO: This is a non-trivial syscall doing some locking, so this is really nasty and may go seriously wrong." |
| `src/system/kernel/thread.cpp` | 305, 478, 483, 488, 932, 1028 | **Truncation** | `snprintf` return values are ignored. If the name is longer than the buffer (e.g. `B_OS_NAME_LENGTH`), it will be silently truncated, potentially causing confusion in debug tools. |

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
