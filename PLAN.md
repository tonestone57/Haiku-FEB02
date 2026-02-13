# Plan to fix remaining Medium Priority Bugs

The following medium priority bugs listed in `BUGS.md` remain to be addressed:

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
