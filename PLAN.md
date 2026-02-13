# Plan to fix remaining bugs in src/system/kernel

## Medium Priority

1.  **`src/system/kernel/fs/vfs.cpp`**: `acquire_advisory_lock` (Deadlock detection)
    *   **Issue:** Deadlock detection is deferred. Local file locking deadlocks are possible.
    *   **Plan:**
        *   Implement a lock dependency graph.
        *   When a thread tries to acquire a lock and has to wait, add an edge to the graph.
        *   Check for cycles in the graph. If a cycle is detected, return `EDEADLK`.
        *   Clean up edges when locks are released or acquired.

2.  **`src/system/kernel/fs/vfs.cpp`**: `disconnect_mount_or_vnode_fds` (Blocking Read/Write)
    *   **Issue:** There is no means to stop a blocking read/write when disconnecting FDs.
    *   **Plan:**
        *   Maintain a list of active I/O requests associated with each file descriptor or vnode.
        *   When `disconnect_mount_or_vnode_fds` is called, iterate through active requests.
        *   Interrupt the threads blocked on these requests (e.g., using `thread_interrupt` or waking them up with a specific error status).
        *   Ensure I/O paths check for this interruption/disconnection status.

3.  **`src/system/kernel/disk_device_manager/KDiskDeviceManager.cpp`**: `ScanPartition` (Locking Issue)
    *   **Issue:** Locking the DDM while scanning partitions is suboptimal.
    *   **Plan:**
        *   Refactor `ScanPartition` to avoid holding the DDM lock for the entire duration.
        *   Mark the partition as "busy" or "scanning".
        *   Create a temporary clone or snapshot of the `partition_data` to pass to the disk system for scanning.
        *   Update the original partition with results after scanning, ensuring atomicity where required.

## Low Priority / Code Quality / Potential Issues

4.  **`src/system/kernel/port.cpp`**: `create_port` (Manual Locking)
    *   **Issue:** `create_port` purportedly uses `mutex_lock` manually.
    *   **Status:** Current code uses `MutexLocker`. Bug might be stale or refers to specific internal calls like `get_locked_port` which uses manual locking for valid reasons (returning locked object).
    *   **Plan:** Verify if `get_locked_port` usage is safe and idiomatic. If so, close as invalid. If not, refactor to use RAII where possible or document the exception.

5.  **`src/system/kernel/device_manager/legacy_drivers.cpp`**: `SetHooks` (Code Smell)
    *   **Issue:** `// TODO: setup compatibility layer!`
    *   **Plan:** Implement a compatibility layer for legacy drivers if the new driver API requires adaptation for hooks like `select`/`deselect` or `readv`/`writev` if they were supported in older versions but handled differently.

6.  **`src/system/kernel/device_manager/legacy_drivers.cpp`**: Hot-reloading (Missing Feature)
    *   **Issue:** `// TODO: a real in-kernel DPC mechanism would be preferred...` and driver reloading logic.
    *   **Plan:**
        *   Implement a Deferred Procedure Call (DPC) mechanism to handle driver events outside of interrupt context/critical sections properly.
        *   Improve `reload_driver` to handle active devices more gracefully.

7.  **`src/system/kernel/device_manager/IOCache.cpp`**: Optimization
    *   **Issue:** Optimization opportunities for read requests and low memory.
    *   **Plan:**
        *   Implement the TODO: "If this is a read request and the missing pages range doesn't intersect with the request, just satisfy the request and don't read anything at all." in `_TransferRequestLine`.
        *   Implement the TODO: "When memory is low, we should consider cannibalizing ourselves or simply transferring past the cache!" in `_TransferRequestLine`.

8.  **`src/system/kernel/fs/vfs.cpp`**: Concurrency
    *   **Issue:** Deadlock detection/lock joining.
    *   **Plan:** Covered by the Medium priority deadlock detection plan. Ensure lock joining (joining locks from same team) is handled.

9.  **`src/system/kernel/port.cpp`**: Resource Management
    *   **Issue:** "TODO: add per team limit" in `get_port_message`.
    *   **Plan:**
        *   Add a per-team counter for committed port space.
        *   Check against a per-team limit (e.g. `kTeamSpaceLimit`) in `get_port_message`.
        *   Increment/decrement the counter accordingly.

10. **`src/system/kernel/sem.cpp`**: Cleanup
    *   **Issue:** `B_CHECK_PERMISSION` should be private.
    *   **Plan:**
        *   Move `B_CHECK_PERMISSION` definition from public headers (if possible/safe) or define a private alias in a private kernel header.
        *   Update kernel code to use the private flag/alias.
        *   Ensure userland syscalls filter or handle this flag correctly.

11. **`src/system/kernel/arch/ppc/paging/460/PPCVMTranslationMap460.cpp`**: Race Condition
    *   **Issue:** "Obvious race condition: Between querying and unmapping the page could have been accessed."
    *   **Plan:**
        *   Analyze the race window in `ClearAccessedAndModified`.
        *   Potential fix: Loop checking the accessed bit. Unmap, then check accessed bit again. If accessed, retry (clear/unmap again) or accept the race if benign for the specific use case (page aging).
