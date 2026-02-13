# Bugs in src/system/kernel

This file lists potential bugs found in the kernel source code, categorized by severity and type.

## Medium Priority

| File | Line | Type | Description |
|---|---|---|---|
| `src/system/kernel/fs/vfs.cpp` | 1992 | **Missing Feature** | `disconnect_mount_or_vnode_fds`: "TODO: there is currently no means to stop a blocking read/write!". `select`/`poll` waiters are notified, but blocking syscalls might still hang. |
