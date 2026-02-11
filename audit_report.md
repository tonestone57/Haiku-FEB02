# Code Audit Report: Haiku `src/servers`

**Date:** 2024
**Target:** `src/servers`
**Auditor:** Jules

## 1. Executive Summary

A comprehensive code audit was performed on `src/servers` subdirectories including `app`, `net`, `media`, `keystore`, `registrar`, `mail`, `syslog_daemon`, `bluetooth`, `debug`, `index`, `input`, `launch`, `midi`, `mount`, `notification`, `package`, `power`, `print`, `print_addon`, and `media_addon`.

The audit identified **3 Critical Vulnerabilities** (stack buffer overflows, missing encryption) and several **High/Medium Risks** (unsafe string handling, race conditions).

## 2. Critical Vulnerabilities

### 2.1. Stack Buffer Overflow in `DHCPClient`
**File:** `src/servers/net/DHCPClient.cpp`
**Function:** `dhcp_message::PutOption` (Line 365)
**Description:**
The `PutOption` function blindly copies data into the `options` array without checking if there is sufficient space remaining in the buffer.
**Status:** Fixed. Added bounds check.

### 2.2. Stack Buffer Overflow in `DWindowHWInterface`
**File:** `src/servers/app/drawing/interface/virtual/DWindowHWInterface.cpp`
**Function:** `_OpenAccelerant`
**Description:**
The function concatenates user-controlled strings into a fixed-size `path` buffer without bounds checking.
**Status:** Fixed. Replaced `sprintf`/`strcat` with `snprintf`/`strlcat`.

### 2.3. Missing Encryption in `KeyStoreServer`
**File:** `src/servers/keystore/Keyring.cpp`
**Function:** `_EncryptToFlatBuffer`
**Description:**
The code responsible for encrypting the keyring data is unimplemented (TODO).
**Impact:** Information Disclosure. Passwords stored in cleartext.
**Status:** Fixed. Implemented AES-256-GCM encryption with Argon2id/PBKDF2 key derivation using OpenSSL.

## 3. High/Medium Risks

### 3.1. Unsafe String Handling in `Registrar`
**File:** `src/servers/registrar/TRoster.cpp`, `src/servers/registrar/ShutdownProcess.cpp`
**Description:**
Use of `strcpy` for copying `app_info` strings.
**Status:** Fixed. Replaced with `strlcpy`.

### 3.2. Race Condition in `StackAndTile`
**File:** `src/servers/app/stackandtile/StackAndTile.cpp`
**Description:**
Race condition where `WindowAdded` might be called concurrently, creating duplicate `SATWindow` objects.
**Status:** Fixed. Implemented existence check and lazy initialization.

### 3.3. Unsafe String Handling in `Package` Server
**File:** `src/servers/package/CommitTransactionHandler.cpp`
**Line:** 1896
**Description:** `strcpy` used to copy package filename.
**Status:** Fixed. Replaced with `strlcpy`.

### 3.4. Unsafe String Handling in `Media Addon` Server
**File:** `src/servers/media_addon/MediaAddonServer.cpp`
**Line:** 724
**Description:** `strcpy` used to copy flavor name.
**Status:** Fixed. Replaced with `strlcpy`.

### 3.5. Unsafe String Handling in `Debug` Server
**File:** `src/servers/debug/DebugServer.cpp`
**Line:** 839
**Description:** `sprintf` used with `%s` on message buffer.
**Status:** Fixed. Replaced with `snprintf`.

### 3.6. Unsafe String Handling in `Input` Server
**File:** `src/servers/input/AddOnManager.cpp`
**Line:** 933
**Description:** `sprintf` used for error message formatting.
**Status:** Fixed. Replaced with `snprintf`.

## 4. Recommendations

1.  **Immediate Fixes:**
    *   Fix remaining buffer overflows in `package`, `media_addon`, `debug`, and `input` servers.
    *   Continue replacing `sprintf`/`strcpy` with `snprintf`/`strlcpy` across the codebase.

2.  **Long-term:**
    *   Implement encryption for `KeyStoreServer`.
    *   Refactor memory management in `ClientMemoryAllocator`.

---

# Code Audit Report: Haiku `src/system/kernel`

This report details bugs found during a static analysis audit of the Haiku kernel source code (`src/system/kernel`).

## 1. `rw_lock` Downgrade Failure

**Severity:** High
**File:** `src/system/kernel/locks/lock.cpp`
**Function:** `_rw_lock_write_unlock`

### Description
The Haiku kernel `rw_lock` implementation supports recursive locking and allows a thread holding a write lock to acquire a read lock (downgrading semantics). However, the implementation of `_rw_lock_write_unlock` has a logic error when downgrading if there are no other threads waiting on the lock.

When `_rw_lock_write_unlock` is called, it subtracts `RW_LOCK_WRITER_COUNT_BASE` from `lock->count`. If `lock->count` contained only the writer count (no readers and no waiters), the result `oldCount` becomes 0.

```cpp
	int32 oldCount = atomic_add(&lock->count, -RW_LOCK_WRITER_COUNT_BASE);
	oldCount -= RW_LOCK_WRITER_COUNT_BASE;

	if (oldCount != 0) {
		// ... logic to preserve readers ...
	}
```

If `oldCount` is 0, the block that handles transferring the `owner_count` (which tracks the nested read lock held by the writer) to `lock->active_readers` is skipped. Additionally, `lock->holder` is set to -1. As a result, the lock effectively enters a completely unlocked state, despite the thread believing it still holds a read lock.

### Consequence
When the thread subsequently calls `rw_lock_read_unlock`, it will attempt to unlock a lock it does not hold (according to the lock's state). This will lead to `lock->active_readers` underflowing (becoming negative) and triggering a kernel panic: `"rw_lock_read_unlock(): lock %p not read-locked"`.

## 2. Integer Overflow in File Descriptor Position Update

**Severity:** Medium
**File:** `src/system/kernel/fs/fd.cpp`
**Functions:** `common_user_io`, `common_vector_io`

### Description
The functions responsible for updating the file descriptor position after a read/write operation perform an addition that is susceptible to integer overflow.

```cpp
	if (movePosition) {
		descriptor->pos = ... : pos + length;
	}
```

`pos` is of type `off_t` (signed 64-bit integer), and `length` is `size_t` (unsigned). If `pos` is a large positive value and `length` is sufficiently large, `pos + length` can overflow and wrap around to a negative value.

### Consequence
If `descriptor->pos` becomes negative (e.g., -1), subsequent I/O operations that rely on the current position (implied `pos = -1`) might misinterpret the position or return errors like `ESPIPE` or `B_BAD_VALUE`, or pass an invalid negative offset to the underlying file system, potentially causing data corruption or unexpected behavior.

## 3. Double Unlock / Race Condition in `scheduler_start`

**Severity:** Medium
**File:** `src/system/kernel/scheduler/scheduler.cpp`
**Function:** `scheduler_start`

### Description
The `scheduler_start` function uses `InterruptsSpinLocker` to acquire the current thread's `scheduler_lock` before calling `reschedule()`.

```cpp
void
scheduler_start()
{
	InterruptsSpinLocker _(thread_get_current_thread()->scheduler_lock);
	SCHEDULER_ENTER_FUNCTION();

	reschedule(B_THREAD_READY);
}
```

When `reschedule()` switches to another thread, the lock of the *old* thread (the one calling `reschedule`) is released by the *new* thread (in `thread_resumes`). When the original thread is eventually rescheduled back, it resumes execution. `reschedule()` returns, and `scheduler_start()` returns.

At this point, `scheduler_lock` is *not* held by the current thread (it was released by the thread that switched to it). However, the `InterruptsSpinLocker` destructor runs and attempts to release the lock again.

### Consequence
Releasing a spinlock that is not held (or held by another CPU) is a race condition. If another CPU has concurrently acquired this thread's `scheduler_lock` (e.g., to adjust priority), the erroneous release in `scheduler_start` will clear the lock, breaking mutual exclusion. This can lead to scheduler data corruption.

## 4. Integer Overflow in `_vm_map_file` Check

**Severity:** High
**File:** `src/system/kernel/vm/vm.cpp`
**Function:** `_vm_map_file`

### Description
In `_vm_map_file`, a boundary check intended to prevent mapping beyond the cache's virtual end is susceptible to integer overflow.

```cpp
	if (mapping != REGION_PRIVATE_MAP && (cache->virtual_base > offset
			|| PAGE_ALIGN(cache->virtual_end) < (off_t)(offset + size))) {
```

`offset` is `off_t` (signed 64-bit) and `size` is `size_t` (unsigned). If `offset` is large, `offset + size` can overflow `off_t` (wrap around to negative), causing the check `PAGE_ALIGN(...) < (off_t)(...)` to unexpectedly fail (return false), allowing the mapping to proceed.

### Consequence
This allows shared mappings to extend beyond the file size (or cache size), potentially leading to access of uninitialized memory or out-of-bounds pages.

## 5. `ring_buffer` Size Mismatch / Integer Overflow

**Severity:** Medium
**File:** `src/system/kernel/util/ring_buffer.cpp`
**Function:** `space_left_in_buffer`

### Description
The function `space_left_in_buffer` returns `int32`, but the ring buffer size (`buffer->size`) is `size_t`.

```cpp
static inline int32
space_left_in_buffer(struct ring_buffer *buffer)
{
	return buffer->size - buffer->in;
}
```

If `buffer->size` exceeds `INT_MAX` (2GB), the result of the subtraction, when cast to `int32`, can become negative.

### Consequence
This negative result propagates to `write_to_buffer`, where checks like `if (length > left)` behave incorrectly, potentially leading to buffer overflows or data corruption when using large ring buffers (> 2GB).

## 6. `RadixBitmap` Integer Truncation / Undefined Behavior

**Severity:** Medium
**File:** `src/system/kernel/util/RadixBitmap.cpp`
**Function:** `radix_leaf_alloc`

### Description
`radix_bitmap_alloc` takes `uint32 count`, but internally `radix_leaf_alloc` (and `radix_node_alloc`) accept `int32 count`.

```cpp
static radix_slot_t
radix_leaf_alloc(radix_node *leaf, radix_slot_t slotIndex, int32 count)
{
	if (count <= (int32)BITMAP_RADIX) {
		// ...
		uint32 n = BITMAP_RADIX - count;
		bitmap_t mask = (bitmap_t)-1 >> n;
```

If `count` is large (>= 2^31), it is treated as negative. This causes `n` to be greater than the width of the type, leading to undefined behavior in the shift operation.

### Consequence
Unpredictable allocation behavior or crashes when requesting very large contiguous slot ranges.

## 7. Infinite Loop in `block_alloc`

**Severity:** High
**File:** `src/system/kernel/slab/allocator.cpp`
**Function:** `block_alloc`

### Description
The logic to align the allocation size contains a potential infinite loop if `alignment` overflows.

```cpp
		while (alignment < size)
			alignment <<= 1;
```

If `alignment` overflows `size_t` (wraps to 0), the condition `0 < size` remains true (assuming `size > 0`), causing an infinite loop.

### Consequence
Denial of service (kernel hang) if a very large alignment is requested (or computed).

## 8. Integer Overflow in `MemoryManager::AllocateRaw`

**Severity:** Medium
**File:** `src/system/kernel/slab/MemoryManager.cpp`
**Function:** `MemoryManager::AllocateRaw`

### Description
The function aligns the requested size using `ROUNDUP`.

```cpp
	size = ROUNDUP(size, SLAB_CHUNK_SIZE_SMALL);
```

If `size` is close to `SIZE_MAX`, `ROUNDUP` can overflow, resulting in a small `size`. The function then allocates a small buffer but returns it for a large request.

### Consequence
Heap buffer overflow. The caller believes it has allocated a large buffer, but the underlying allocation is small.

## 9. Kernel Stack Overflow in `KDiskDeviceManager::_Scan`

**Severity:** High
**File:** `src/system/kernel/disk_device_manager/KDiskDeviceManager.cpp`
**Function:** `_Scan`

### Description
The `_Scan` function recursively scans directories in `/dev/disk`. It allocates a `KPath` object (1024 bytes) on the stack in each frame.

```cpp
			KPath entryPath;
// ...
			if (_Scan(entryPath.Path()) == B_OK)
```

With a limited kernel stack (typically 16KB), a directory depth of ~10-15 is sufficient to cause a stack overflow.

### Consequence
Kernel panic or crash due to stack overflow if a deep directory structure exists in `/dev/disk` (e.g., created by a malicious user or mounted filesystem).

## 10. `VMUserAddressSpace::UnreserveAddressRange` Silent Failure

**Severity:** Low
**File:** `src/system/kernel/vm/VMUserAddressSpace.cpp`
**Function:** `UnreserveAddressRange`

### Description
When unreserving a range, the function iterates over areas.

```cpp
	for (... (area = it.Next()) != NULL
			&& area->Base() + area->Size() - 1 <= endAddress;) {
```

If the existing reserved area is larger than the requested unreserve range (i.e., unreserving a sub-range), the condition fails, the loop terminates, and the function returns `B_OK` without doing anything.

### Consequence
The address range remains reserved, but the caller assumes it has been unreserved. This logic error prevents partial unreservation of ranges.

## 11. Integer Overflow in `dir_vnode_to_path`

**Severity:** High
**File:** `src/system/kernel/fs/vfs.cpp`
**Function:** `dir_vnode_to_path`

### Description
The function uses an `int32` index to write into the path buffer.

```cpp
	int32 insert = bufferSize;
// ...
	path[--insert] = '\0';
```

If `bufferSize` exceeds `INT_MAX` (2GB), `insert` becomes negative. Accessing `path[--insert]` results in a write significantly before the start of the allocated buffer.

### Consequence
Heap corruption or crash due to out-of-bounds write when `vfs_normalize_path` is called with a very large buffer size.
