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
**Status:** Fixed.
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
**Status:** Fixed.
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
**Status:** Fixed.
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
**Status:** Fixed.
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

## 12. Integer Overflow in `Allocator::AllocateAligned`

**Severity:** Medium
**File:** `src/system/kernel/debug/core_dump.cpp`
**Function:** `Allocator::AllocateAligned`

### Description
The calculation for `fAlignedSize` is susceptible to integer overflow.

```cpp
	fAlignedSize += (size + 7) / 8 * 8;
```

If `size` is close to `SIZE_MAX`, `size + 7` overflows, leading to a much smaller allocation than required. This is also present in `AllocateString` where `length + 1` is used.

### Consequence
Heap corruption or buffer overflow during core dump generation if very large allocations are requested.

## 13. Integer Overflow in `gdb_parse_command` Address Parsing

**Severity:** Low
**File:** `src/system/kernel/debug/gdb.cpp`
**Function:** `gdb_parse_command`

### Description
The loop parsing hexadecimal addresses does not check for overflow.

```cpp
		address <<= 4;
		address += parse_nibble(*ptr);
```

If a malicious or malformed GDB command provides an address string longer than the pointer width (e.g., > 16 hex digits on 64-bit), the address will silently wrap around.

### Consequence
Incorrect address access during kernel debugging sessions.

## 14. Missing NULL Check in `PrecacheIO::Prepare`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/cache/file_cache.cpp`
**Function:** `PrecacheIO::Prepare`

### Description
The function calls `vm_page_allocate_page` but does not check if the returned pointer is `NULL`.

```cpp
		vm_page* page = vm_page_allocate_page(reservation, ...);
		fCache->InsertPage(page, fOffset + pos);
```

If memory allocation fails (despite reservation logic, which might fail or block indefinitely in other scenarios), `InsertPage` will dereference the `NULL` pointer.

### Consequence
Kernel panic (null pointer dereference).

## 15. Missing NULL Check in `read_into_cache`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/cache/file_cache.cpp`
**Function:** `read_into_cache`

### Description
Similar to `PrecacheIO::Prepare`, `read_into_cache` allocates pages in a loop and immediately uses them without checking for `NULL`.

```cpp
		vm_page* page = pages[pageIndex++] = vm_page_allocate_page(...);
		cache->InsertPage(page, offset + pos);
```

### Consequence
Kernel panic (null pointer dereference).

## 16. Missing NULL Check in `write_to_cache`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/cache/file_cache.cpp`
**Function:** `write_to_cache`

### Description
The function allocates pages for writing but fails to check for `NULL` return from `vm_page_allocate_page`.

```cpp
		vm_page* page = pages[pageIndex++] = vm_page_allocate_page(...);
		page->modified = !writeThrough;
```

### Consequence
Kernel panic (null pointer dereference).

## 17. Kernel Panic on Partial Page Read Error in `write_to_cache`

**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/cache/file_cache.cpp`
**Function:** `write_to_cache`

### Description
When performing a partial write that requires reading the rest of the page from disk, the function panics if the read fails.

```cpp
		status = vfs_read_pages(...);
		if (status < B_OK)
			panic("1. vfs_read_pages() failed: %s!\n", strerror(status));
```

A simple I/O error (e.g., bad sector) should return an error code to the caller, not panic the entire kernel.

### Consequence
Denial of service (kernel panic) triggered by disk I/O errors.

## 18. Integer Overflow in `cache_prefetch_vnode` Heuristic

**Severity:** Low
**Status:** Fixed.
**File:** `src/system/kernel/cache/file_cache.cpp`
**Function:** `cache_prefetch_vnode`

### Description
The heuristic calculation for prefetching can overflow `uint32`.

```cpp
	if (offset >= fileSize || vm_page_num_unused_pages() < 2 * pagesCount
		|| (3 * cache->page_count) > (2 * fileSize / B_PAGE_SIZE)) {
```

`cache->page_count` is `uint32`. If the cache grows large enough (approx. 1.4 billion pages, ~5.6 TB), `3 * cache->page_count` will overflow, potentially causing incorrect prefetching behavior.

### Consequence
Incorrect cache prefetching decisions on systems with extremely large memory/caches.

## 19. Unsafe Downcast in `file_cache_create`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/cache/file_cache.cpp`
**Function:** `file_cache_create`

### Description
The function retrieves a `VMCache` and blindly casts it to `VMVnodeCache*`.

```cpp
	if (vfs_get_vnode_cache(ref->vnode, &ref->cache, true) != B_OK)
		goto err1;
	// ...
	((VMVnodeCache*)ref->cache)->SetFileCacheRef(ref);
```

If `vfs_get_vnode_cache` returns a different cache type (e.g., if the vnode is using a device cache or other mechanism), this cast is invalid and calling `SetFileCacheRef` (which might not exist or be at a different offset) causes memory corruption.

### Consequence
Kernel crash or memory corruption.

## 20. Dangling Pointer in `VMVnodeCache` after `file_cache_delete`

**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/cache/file_cache.cpp`
**Function:** `file_cache_delete`

### Description
`file_cache_delete` destroys the `file_cache_ref` but does not explicitly NULL out the reference in the associated `VMVnodeCache`.

```cpp
	ref->cache->ReleaseRef();
	delete ref;
```

If the `VMVnodeCache` survives (due to other references), it retains a dangling pointer to the deleted `file_cache_ref`. Subsequent calls like `cache_prefetch_vnode` which access `((VMVnodeCache*)cache)->FileCacheRef()` will dereference this invalid pointer.

### Consequence
Use-after-free, leading to kernel panic or corruption.

## 21. Double Child Removal in `device_node` Destructor

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/device_manager/device_manager.cpp`
**Function:** `~device_node`

### Description
The destructor of `device_node` iterates over its children to delete them.

```cpp
	while (device_node* child = fChildren.RemoveHead()) {
		delete child;
	}
```

However, the child's destructor (`~device_node`) calls `Parent()->RemoveChild(this)`.

```cpp
void device_node::RemoveChild(device_node* node) {
	// ...
	fChildren.Remove(node);
	Release();
}
```

Since the parent has already removed the child from `fChildren` using `RemoveHead()`, the subsequent `fChildren.Remove(node)` call in `RemoveChild` operates on a node that is no longer in the list (or corruption ensues if pointers weren't cleared). This double removal is unsafe.

### Consequence
List corruption, double-free, or crash during device node teardown.

## 22. Panic on User Memory Mapping Failure in `IORequest::_CopyUser`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/device_manager/IORequest.cpp`
**Function:** `IORequest::_CopyUser`

### Description
The function attempts to map user memory for copying.

```cpp
		status_t error = get_memory_map_etc(team, external, size, entries, &count);
		if (error != B_OK && error != B_BUFFER_OVERFLOW) {
			panic("IORequest::_CopyUser(): Failed to get physical memory for "
				"user memory %p\n", external);
```

If `get_memory_map_etc` fails (e.g., memory not locked, invalid address, or paged out), the kernel panics. This allows a malicious user or buggy driver to crash the system by providing an invalid buffer or failing to lock memory.

### Consequence
Denial of service (kernel panic).

## 23. Stack Overflow via Recursion in `TraceFilterParser`

**Severity:** Low
**File:** `src/system/kernel/debug/tracing.cpp`
**Function:** `TraceFilterParser::_ParseExpression`

### Description
The parser for trace filters uses recursion for `not`, `and`, and `or` expressions without a depth limit.

```cpp
		} else if (strcmp(token, "not") == 0) {
			// ...
			if ((filter->fSubFilters.first = _ParseExpression()) != NULL)
```

A sufficiently deep filter expression (e.g., `not not not ...`) provided via the kernel debugger command line can exhaust the kernel stack.

### Consequence
Kernel stack overflow (crash) triggered by debugger command.

## 24. Integer Overflow in `switch_sem_etc` Timeout Calculation

**Severity:** Medium
**File:** `src/system/kernel/sem.cpp`
**Function:** `switch_sem_etc`

### Description
When calculating the absolute timeout for `thread_block_with_timeout`, the code adds `system_time()` to the relative `timeout` value.

```cpp
	if ((flags & B_RELATIVE_TIMEOUT) != 0
		&& timeout != B_INFINITE_TIMEOUT && timeout > 0) {
		// ...
		timeout += system_time();
	}
```

If `timeout` is a very large positive value (close to `B_INFINITE_TIMEOUT` but not equal), the addition can overflow `bigtime_t` (int64), resulting in a negative value (representing a time in the past). This causes `thread_block_with_timeout` to timeout immediately.

### Consequence
Premature timeout for semaphore acquisition when using very large timeout values.

## 25. Logic Error in `switch_sem_etc` Missing Semaphore Release

**Severity:** Critical
**Status:** Fixed.
**File:** `src/system/kernel/sem.cpp`
**Function:** `switch_sem_etc`

### Description
The `switch_sem_etc` function is intended to atomically acquire one semaphore and release another. If the acquisition blocks, it correctly releases the `semToBeReleased`. However, if the acquisition succeeds *immediately* (non-blocking path), the function fails to release `semToBeReleased`.

```cpp
	if ((sSems[slot].u.used.count -= count) < 0) {
        // ... (blocking path) ...
		if (semToBeReleased >= 0) {
			release_sem_etc(semToBeReleased, 1, B_DO_NOT_RESCHEDULE);
			semToBeReleased = -1;
		}
        // ...
	} else {
        // ... (success path) ...
        // MISSING release_sem_etc(semToBeReleased, ...)
	}
```

### Consequence
The semaphore that should have been released remains held. This breaks the atomic "switch" semantics and leads to deadlocks and leaked semaphore counts.

## 26. Integer Overflow in `load_image_etc` Allocation

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/team.cpp`
**Function:** `load_image_etc`

### Description
The function calculates the size required for flattened arguments using signed `int32` arithmetic.

```cpp
	int32 size = (argCount + envCount + 2) * sizeof(char*) + argSize + envSize;
```

If `argCount` is large enough, `(argCount + envCount + 2) * sizeof(char*)` can overflow `int32`, resulting in a negative or small positive value. `malloc` (taking `size_t`) allocates a small buffer. The subsequent loops then copy `argCount` pointers, writing well beyond the end of the allocated buffer.

### Consequence
Heap buffer overflow, potentially leading to privilege escalation or kernel crash.

## 27. Integer Overflow in `copy_user_process_args` Check

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/team.cpp`
**Function:** `copy_user_process_args`

### Description
The validation check for the required buffer size is vulnerable to integer overflow.

```cpp
	if ((argCount + envCount + 2) * sizeof(char*) > flatArgsSize
```

Similar to Bug 26, `(argCount + envCount + 2)` is calculated as `int32`. If it overflows to negative, the comparison against `flatArgsSize` (unsigned `size_t`) might behave unexpectedly or allow a small `flatArgsSize` to pass check, while the subsequent code assumes the buffer is large enough to hold all pointers.

### Consequence
Heap buffer overflow.

## 28. ASLR Setting Lost in `fork_team`

**Severity:** Low
**File:** `src/system/kernel/team.cpp`
**Function:** `fork_team`

### Description
When forking a team, the `DISABLE_ASLR` flag (or ASLR state) is not propagated to the child team's address space.

```cpp
	// create an address space for this team
	status = VMAddressSpace::Create(..., &team->address_space);
```

Unlike `load_image_internal`, which explicitly calls `SetRandomizingEnabled` based on arguments, `fork_team` uses the default (enabled). If the parent had ASLR disabled, the child will unexpectedly have it enabled (or vice versa depending on default).

### Consequence
Inconsistent process state; debuggers or tools relying on disabled ASLR might fail on forked children.

## 29. Zombie Process Leak in `team_get_death_entry`

**Severity:** Medium
**File:** `src/system/kernel/team.cpp`
**Function:** `team_get_death_entry`

### Description
The logic for removing a death entry seems flawed.

```cpp
		if (team_get_current_team_id() == entry->thread) {
			team->dead_children.entries.Remove(entry);
            // ...
			*_deleteEntry = true;
		}
```

It checks if the *caller's* team ID matches the *dead child's* thread ID (`entry->thread`). This is only true if a thread is waiting for itself (impossible/deadlock) or the IDs happen to collide (unlikely). The intention is likely to check if the caller is the *parent* of the dead child (which is `team`), but the check `team_get_current_team_id() == entry->thread` prevents the entry from being removed in `wait_for_thread`.

### Consequence
`wait_for_thread` fails to reap the zombie entry from the dead children list, causing a resource leak (zombie entries accumulate).

## 30. Infinite Loop / Logic Error in `get_next_team_info` ID Wrap

**Severity:** Low
**File:** `src/system/kernel/team.cpp`
**Function:** `_get_next_team_info`

### Description
The function iterates through team IDs using a `lastTeamID` retrieved from `peek_next_thread_id()`.

```cpp
	team_id lastTeamID = peek_next_thread_id();
    // ...
	while (slot < lastTeamID && ...)
```

If team IDs wrap around (which they do), `lastTeamID` might be smaller than `slot` (the cookie), causing the loop to terminate prematurely, making some teams invisible to iteration.

### Consequence
System monitoring tools might fail to list all teams after ID wraparound.

## 31. Information Leak in `writev_port_etc`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/port.cpp`
**Function:** `writev_port_etc`

### Description
The function allocates a buffer based on `bufferSize` but fills it from `msgVecs`.

```cpp
	port_message* message = ... malloc(sizeof(port_message) + bufferSize);
    // ...
	if (bufferSize > 0) {
		for (uint32 i = 0; i < vecCount; i++) {
            // ... copy vecs ...
			bufferSize -= bytes;
			if (bufferSize == 0) break;
		}
	}
```

If the provided IO vectors sum to less than `bufferSize`, the remaining bytes in `message->buffer` (kernel heap memory) are not initialized. `message->size` is set to the original `bufferSize`. A reader will receive this uninitialized kernel data.

### Consequence
Kernel memory information disclosure to unprivileged users.

## 32. DoS Vector in `create_port` Team Limit Check

**Severity:** Low
**File:** `src/system/kernel/port.cpp`
**Function:** `create_port`

### Description
The function enforces a per-team port limit by iterating the list.

```cpp
		if (list_count_items(&team->port_list) >= 4096)
```

`list_count_items` is O(N). Holding the team list lock while iterating 4096 items during every port creation creates a performance bottleneck and potential DoS vector if many threads create ports simultaneously.

### Consequence
Performance degradation / CPU consumption.

## 33. Unsafe Kernel Copy in `send_data`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/thread.cpp`
**Function:** `send_data` / `send_data_etc`

### Description
`send_data` (kernel API) calls `send_data_etc` which uses `user_memcpy`.

```cpp
		if (user_memcpy(data, buffer, bufferSize) != B_OK)
```

If `send_data` is used for kernel-to-kernel messaging (where `buffer` is a kernel address), `user_memcpy` (which typically enforces user address ranges or uses special instructions) might fail or panic, preventing kernel threads from communicating efficiently or causing crashes.

### Consequence
Kernel malfunction or crash when using thread messaging between kernel threads.

## 34. Unsafe Kernel Copy in `receive_data`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/thread.cpp`
**Function:** `receive_data` / `receive_data_etc`

### Description
Similar to Bug 33, `receive_data_etc` uses `user_memcpy` to copy the message to the receiver's buffer.

```cpp
		status = user_memcpy(buffer, thread->msg.buffer, size);
```

If the receiver is a kernel thread providing a kernel buffer, this copy operation is incorrect/unsafe.

### Consequence
Kernel malfunction or crash.

## 35. Stack Overflow in `thread_create_user_stack`

**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/thread.cpp`
**Function:** `create_thread_user_stack`

### Description
The function calculates `areaSize` adding a user-provided `guard_size`.

```cpp
	size_t areaSize = PAGE_ALIGN(guardSize + stackSize + TLS_SIZE + additionalSize);
```

`guardSize` comes from `attributes.guard_size` and is not checked for upper bounds (unlike `stackSize`). If a user provides a very large `guard_size` (close to `SIZE_MAX`), `areaSize` calculation wraps around to a small value. `create_area_etc` succeeds with a small area. When the thread tries to use the stack (expecting a large guard + stack), it will immediately overflow the area.

### Consequence
Stack overflow / memory corruption.

## 36. Integer Overflow in `KMessage::_CapacityFor`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/messaging/KMessage.cpp`
**Function:** `KMessage::_CapacityFor`

### Description
The function calculates capacity using `(size + 63) / 64 * 64`. If `size` is close to `INT_MAX`, `size + 63` overflows `int32` (signed), resulting in a small or negative value. This leads to allocating a buffer too small for the data, causing heap overflow in `_AllocateSpace`.

### Consequence
Heap buffer overflow.

## 37. Integer Truncation in `XsiSemaphoreSet::RecordUndo`

**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/posix/xsi_semaphore.cpp`
**Function:** `XsiSemaphoreSet::RecordUndo`

### Description
The result of adding the undo value is stored in `int newValue`, checked against `USHRT_MAX` (65535), and then assigned to `int16` array `undo_values`. Values between 32768 and 65535 pass the check but are truncated to negative values in `int16`, corrupting the undo state. The check should use `SHRT_MAX`.

### Consequence
Corruption of semaphore undo state, leading to incorrect semaphore values on process exit.

## 38. Integer Overflow in `elf_load_user_image` Segment Calculation

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/elf.cpp`
**Function:** `elf_load_user_image`

### Description
The calculation `programHeaders[i].p_vaddr + programHeaders[i].p_memsz` can overflow `size_t` (on 32-bit or if 64-bit values are large/malformed). This leads to `memUpperBound` wrapping around, potentially causing `vm_map_file` to map a small area or `create_area` to underflow `bssSize`, leading to memory corruption.

### Consequence
Memory corruption, potential code execution via malformed ELF.

## 39. Integer Overflow in `load_kernel_add_on` Reservation

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/elf.cpp`
**Function:** `load_kernel_add_on`

### Description
The function sums up segment sizes into `reservedSize`. `length += ROUNDUP(...)`. If a kernel add-on has huge segments (malformed), `length` can overflow `ssize_t`. The subsequent `vm_reserve_address_range` might succeed with a smaller size, but loading content will write beyond the reserved area.

### Consequence
Kernel memory corruption.

## 40. Missing Null Termination Check in `_user_register_image`

**Severity:** Medium
**File:** `src/system/kernel/image.cpp`
**Function:** `_user_register_image`

### Description
The function copies `extended_image_info` from user space but does not validate that `basic_info.name` is null-terminated. Subsequent usage of this name (e.g. printing in debugger or log) can read out of bounds.

### Consequence
Information disclosure (OOB read).

## 41. Unsafe User Buffer Access in `_user_get_next_loaded_module_name`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/module.cpp`
**Function:** `_user_get_next_loaded_module_name`

### Description
The function passes a user-space buffer `buffer` directly to `get_next_loaded_module_name`, which calls `strlcpy` on it. This accesses user memory without `user_memcpy` or SMAP protection, leading to potential crashes or security violations.

### Consequence
Kernel crash or security violation (SMAP bypass).

## 42. Missing Ownership Check in `_user_xsi_msgctl`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/posix/xsi_message_queue.cpp`
**Function:** `_user_xsi_msgctl`

### Description
For `IPC_SET` and `IPC_RMID` commands, the function checks `messageQueue->HasPermission()` which requires write permission. However, POSIX specifies that the caller must be the owner (uid/cuid) or privileged. This allows any user with write access to the queue to delete it or change its parameters.

### Consequence
Unauthorized deletion or modification of message queues.

## 43. Missing Ownership Check in `_user_xsi_semctl`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/posix/xsi_semaphore.cpp`
**Function:** `_user_xsi_semctl`

### Description
Similar to `xsi_msgctl`, `IPC_SET` and `IPC_RMID` checks only verify write permissions (`HasPermission`) instead of ownership, allowing unauthorized modification or deletion of semaphore sets.

### Consequence
Unauthorized deletion or modification of semaphore sets.

## 44. Unsafe Kernel Copy in `receive_data_etc`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/thread.cpp`
**Function:** `receive_data_etc`

### Description
The function unconditionally uses `user_memcpy` to copy the message to the receiver's buffer. If the receiver is a kernel thread (using `receive_data`), `user_memcpy` will fail (or panic depending on implementation) when accessing the kernel address `buffer`, breaking kernel-internal messaging.

### Consequence
Kernel malfunction or crash.

## 45. Unsafe Kernel Copy in `send_data_etc`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/thread.cpp`
**Function:** `send_data_etc`

### Description
Similar to `receive_data_etc`, this function unconditionally uses `user_memcpy` to copy data from the sender's buffer. If the sender is a kernel thread, this operation is invalid.

### Consequence
Kernel malfunction or crash.

## 46. Missing Argument Relocation in `team_create_thread_start_internal`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/team.cpp`
**Function:** `team_create_thread_start_internal`

### Description
The function copies `flatArgs` to the user stack `userArgs` using `user_memcpy`. However, `flatArgs` (allocated in kernel) contains pointers that point to addresses within the kernel buffer. These pointers are not relocated to point to the user stack addresses. The user process receives `argv` pointers pointing to kernel memory, which causes a crash or potential info leak.

### Consequence
Process crash or potential kernel memory disclosure.

## 47. Use-After-Free in `unload_module`

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/module.cpp`
**Function:** `unload_module`

### Description
The function looks up `moduleImage`, unlocks `sModulesLock`, and then calls `put_module_image(moduleImage)`. Since the lock is released, another thread can unload and free `moduleImage` before `put_module_image` is called. Accessing `moduleImage` (to decrement ref count) in `put_module_image` is a use-after-free.

### Consequence
Use-after-free, potential kernel crash or corruption.

## 48. Race Condition / Use-After-Free in `swap_file_delete`

**Severity:** Critical
**Status:** Fixed.
**File:** `src/system/kernel/vm/VMAnonymousCache.cpp`
**Function:** `swap_file_delete` / `VMAnonymousCache::Read`

### Description
`swap_file_delete` removes a swap file from `sSwapFileList` and destroys it. `VMAnonymousCache::Read` (and other functions) calls `find_swap_file`, which iterates `sSwapFileList` **without locking**. If `swap_file_delete` runs concurrently with a swap read operation, `find_swap_file` may access invalid memory or `swap_file_delete` may destroy the swap file structure while `Read` is using it.

### Consequence
Kernel crash or memory corruption during swap operations.


**Note:** A regression (Double Lock) introduced by the initial fix for this bug was identified and fixed in `find_swap_file_locked`.
## 49. Infinite Loop / DoS in `vfs_bind_mount_directory`

**Severity:** Critical
**Status:** Fixed.
**File:** `src/system/kernel/fs/vfs.cpp`
**Function:** `vfs_bind_mount_directory`

### Description
The function allows creating arbitrary bind mounts by setting `covers` and `covered_by` pointers. It does not check for cycles. If a cycle is created (e.g., A covers B, B covers A), functions like `get_covering_vnode_locked` (which loop `while (coveringNode->covered_by != NULL)`) will enter an infinite loop.

### Consequence
Denial of Service (kernel hang) triggered by malicious or accidental bind mount configuration.

## 50. Locking Violation / Race in `ThreadTimeUserTimer::Stop`

**Severity:** Critical
**Status:** Fixed.
**File:** `src/system/kernel/UserTimer.cpp`
**Function:** `ThreadTimeUserTimer::Stop`

### Description
The `Stop` function modifies the shared `fScheduled` flag and calls `CancelTimer`. It requires `sUserTimerLock` to be held (as per comments and `Schedule` usage), but it is called from `user_timer_stop_cpu_timers` (via `scheduler_reschedule`) **without** holding the lock. This creates a race condition with `Schedule` running on another CPU, potentially leaving `fScheduled` state inconsistent with the actual kernel timer state.

### Consequence
Timer state corruption, potentially leading to timers never firing or firing after being cancelled (double free risk in hooks).

## 51. Uninterruptible Wait in `vfs_read_pages`

**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/fs/vfs.cpp`
**Function:** `vfs_read_pages`

### Description
The function uses an `IORequest` on the stack and calls `request.Wait()`. `IORequest::Wait` waits with `B_INFINITE_TIMEOUT` and no interruption flags (by default). If the underlying file system or driver hangs (e.g., network timeout, USB stall), the calling thread enters an uninterruptible sleep (D state) and cannot be killed.

### Consequence
Unresponsive system/processes when I/O devices hang.

## 52. Logic Error / Data Miss in `RingBuffer::Read`

**Severity:** Medium
**File:** `src/system/kernel/fs/fifo.cpp`
**Function:** `RingBuffer::Read`

### Description
There is a race condition between `fWriteHead` update and `fWriteAvailable` update in `Write`. The `Read` function checks `fWriteAvailable == 0` to determine if the buffer is empty when `readEnd == readHead`. If `Write` updates `fWriteHead` (wrapping it to match `readHead`) but hasn't yet decremented `fWriteAvailable`, `Read` sees `readEnd == readHead` and `fWriteAvailable > 0`. It interprets this as "empty" (or fails to read) even though data is present.

### Consequence
Reader misses available data, potentially causing stalls or higher latency in pipes/FIFOs.

## 53. Busy Wait in `VMAnonymousCache::_SwapBlockBuild`

**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/vm/VMAnonymousCache.cpp`
**Function:** `_SwapBlockBuild`

### Description
The function implements a busy-wait loop to allocate memory.
```cpp
			if (swap == NULL) {
				locker.Unlock();
				snooze(10000);
				locker.Lock();
                // ... continue loop
```
It releases the lock, snoozes, and retries. This can lead to excessive CPU usage or livelocks under memory pressure, and holding `sSwapHashLock` (write lock) aggressively blocks other swap operations.

### Consequence
Performance degradation or livelock under high load/OOM.

## 54. Swap Space Leak in `VMAnonymousCache::_FreeSwapPageRange`

**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/vm/VMAnonymousCache.cpp`
**Function:** `_FreeSwapPageRange`

### Description
The function explicitly skips freeing swap space for "busy" pages to avoid deadlocks.
```cpp
			if (page != NULL && page->busy) {
				// We skip (i.e. leak) swap space of busy pages
				continue;
			}
```
This permanently leaks the allocated swap slots in the `radix_bitmap`.

### Consequence
Gradual exhaustion of swap space availability over time.

## 55. Integer Overflow in `swap_file_add`

**Severity:** Low
**Status:** Fixed.
**File:** `src/system/kernel/vm/VMAnonymousCache.cpp`
**Function:** `swap_file_add`

### Description
The calculation `(off_t)pageCount * B_PAGE_SIZE` can overflow `off_t` if the swap file size exceeds `LLONG_MAX`. While unlikely (requires exabytes), `pageCount` is `uint32`, so it wraps at 16TB (`4096 * 2^32`). If a user adds a swap file > 16TB, `pageCount` wraps, and `sAvailSwapSpace` is updated incorrectly.

### Consequence
Incorrect swap space accounting for extremely large swap files.

## 56. `UserNodeListener` RTTI Usage in Kernel

**Severity:** Low
**File:** `src/system/kernel/fs/node_monitor.cpp`
**Function:** `_RemoveListener`

### Description
The code uses `dynamic_cast<UserNodeListener*>` to identify listener types. `dynamic_cast` relies on RTTI. If the kernel is compiled with `-fno-rtti` (common for kernels), this code is invalid or will fail to link/run correctly. If enabled, it adds unnecessary overhead.

### Consequence
Potential build failure or undefined behavior depending on compiler flags.

## 57. Signal to Kernel Thread in `FIFOInode::Write`

**Severity:** Low
**File:** `src/system/kernel/fs/fifo.cpp`
**Function:** `FIFOInode::Write`

### Description
If a writer attempts to write to a closed pipe, `send_signal(find_thread(NULL), SIGPIPE)` is called. If the writer is a kernel thread (e.g., a kernel service using internal pipes), sending a signal might be unexpected or ignored, or cause termination if not handled.

### Consequence
Potential stability issue for kernel services using FIFOs.

## 58. Bind Mount Renaming Limitation in `common_rename`

**Severity:** Low
**Status:** Fixed.
**File:** `src/system/kernel/fs/vfs.cpp`
**Function:** `common_rename`

### Description
The function checks `if (fromVnode->device != toVnode->device) return B_CROSS_DEVICE_LINK;`. Bind mounts assign new device IDs. Thus, renaming a file within the *same* physical volume but across different bind mount points (or between a bind mount and the original mount) fails, even though it is physically possible.

### Consequence
Unexpected failure of `rename()` in bind-mount scenarios.

## 59. `common_ioctl` Buffer Safety Design Flaw

**Severity:** Medium
**File:** `src/system/kernel/fs/vfs.cpp`
**Function:** `common_ioctl`

### Description
The `common_ioctl` function passes the user-provided `buffer` pointer directly to the file system's `ioctl` hook without validating that it is a valid user address (e.g. using `is_user_address`). While drivers are *supposed* to check this, a central check or `user_memcpy` enforcement would prevent a class of privilege escalation bugs in individual drivers.

### Consequence
Increased attack surface for privilege escalation via buggy drivers.

## 60. Missing Status Update in `do_iterative_fd_io` Error Path

**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/fs/vfs_request_io.cpp`
**Function:** `do_iterative_fd_io`

### Description
In `do_iterative_fd_io`, when the `getVecs` callback returns an error (other than `B_BUFFER_OVERFLOW` and a few others), the function returns the error directly. However, it fails to call `request->SetStatusAndNotify(error)`. Since the request was already created and potentially partially initialized or linked, failing to notify completion may leave the caller hanging indefinitely if it's waiting on the request.

### Consequence
Hung thread or resource leak if I/O iteration fails early.

## 61. Race Condition in `republish_driver`

**Severity:** Medium
**File:** `src/system/kernel/device_manager/legacy_drivers.cpp`
**Function:** `republish_driver`

### Description
The function iterates over `driver->devices` to mark them as not republished, then calls `driver->publish_devices()` to get the new list. If a device is found in the new list, it updates hooks. However, `LegacyDevice::InitDevice` also accesses `fDriver` and `fDriver->devices_used`. There is a potential race where `republish_driver` might be modifying the device list or hooks while another thread is initializing or using a device from the same driver, potentially leading to inconsistent state or access to unloaded code if locking is not perfectly synchronized (global `sLock` is used, but interactions with `devfs` might be complex).

### Consequence
Potential instability during driver updates.

## 62. Unsafe String Copy in `LegacyDevice::Control`

**Severity:** Low
**File:** `src/system/kernel/device_manager/legacy_drivers.cpp`
**Function:** `LegacyDevice::Control`

### Description
The `B_GET_DRIVER_FOR_DEVICE` case uses `user_strlcpy` to copy `fDriver->path`.
```cpp
			if (length != 0 && length <= strlen(fDriver->path))
				return ERANGE;
			return user_strlcpy(static_cast<char*>(buffer), fDriver->path, length);
```
The check `length <= strlen` is correct for ensuring full copy, but if `length` is 0, it returns `ERANGE` (which might be unexpected for 0 length). More importantly, `fDriver->path` might be modified by `add_driver` (reload) concurrently. `sLock` is not held in `Control`.

### Consequence
Potential race reading driver path.

## 63. Integer Overflow in `id_generator`

**Severity:** Low
**File:** `src/system/kernel/device_manager/id_generator.cpp`
**Function:** `create_id_internal`

### Description
The `id_generator` uses a fixed-size bitmap `uint8 alloc_map[(GENERATOR_MAX_ID + 7) / 8]`. `GENERATOR_MAX_ID` is 64. If more than 64 IDs are requested, it returns `B_ERROR`. While not a buffer overflow, it's a very small limit hardcoded in the kernel, which could cause system functionality issues if many devices of the same type are present (e.g., more than 64 partitions or disk devices if they use this generator).

### Consequence
Resource exhaustion (IDs) with relatively low limit.

## 64. Integer Overflow in `DMABuffer::Create`

**Severity:** Medium
**File:** `src/system/kernel/device_manager/dma_resources.cpp`
**Function:** `DMABuffer::Create`

### Description
The allocation size calculation is `sizeof(DMABuffer) + sizeof(generic_io_vec) * (count - 1)`.
```cpp
	DMABuffer* buffer = (DMABuffer*)malloc(
		sizeof(DMABuffer) + sizeof(generic_io_vec) * (count - 1));
```
`count` is passed as `size_t`. If `count` is 0, `count - 1` underflows to `SIZE_MAX`, resulting in a huge allocation size (or overflow back to small if multiplied). `fRestrictions.max_segment_count` usually limits this, but `Init` allows `max_segment_count` to be 0 (then sets default 16). If a driver provides a bogus non-zero attribute, it might trigger this.

### Consequence
Huge allocation failure or heap corruption.

## 65. Integer Overflow in `KPartition::SetSize`

**Severity:** Low
**File:** `src/system/kernel/disk_device_manager/KPartition.cpp`
**Function:** `KPartition::SetSize`

### Description
The function accepts `off_t size`. It does not check if `size` is negative.
```cpp
void KPartition::SetSize(off_t size) {
	if (fPartitionData.size != size) { ... }
}
```
A negative size could cause havoc in calculations like `offset + size`.

### Consequence
Logic errors in partition handling.

## 66. `KDiskDevice::GetMediaStatus` Masks Errors

**Severity:** Low
**File:** `src/system/kernel/disk_device_manager/KDiskDevice.cpp`
**Function:** `KDiskDevice::GetMediaStatus`

### Description
The function calls `ioctl(fFD, B_GET_MEDIA_STATUS, ...)` and `GetGeometry`.
```cpp
	if (error != B_OK) {
        // ... checks geometry ...
			if (!geometry.removable) {
				error = B_OK;
				*mediaStatus = B_OK;
			}
	}
```
If the ioctl fails with a specific error (e.g. `B_DEV_MEDIA_CHANGED` which is not an error but a status, or a real error), and the device is not removable, it forces `B_OK`. It might mask genuine hardware errors.

### Consequence
Masking of I/O errors.

## 67. Buffer Overflow Risk in `KPartition::GetFileName`

**Severity:** Medium
**File:** `src/system/kernel/disk_device_manager/KPartition.cpp`
**Function:** `KPartition::GetFileName`

### Description
The function appends `_index` to the parent name.
```cpp
	size_t len = strlen(buffer);
	if (snprintf(buffer + len, size - len, "_%" B_PRId32, Index()) >= int(size - len))
		return B_NAME_TOO_LONG;
```
It relies on `buffer` containing the parent name. If the parent name is already close to `B_FILE_NAME_LENGTH`, this concatenation logic is safe due to `snprintf` checks, but if `GetFileName` is called recursively for deep partition nesting (partition of a partition of a partition...), the resulting name might exceed standard limits or be truncated/rejected, making the partition unpublishable.

### Consequence
Inability to publish deeply nested partitions.

## 68. Livelock / Excessive Wait in `block_notifier_and_writer`

**Severity:** Medium
**File:** `src/system/kernel/cache/block_cache.cpp`
**Function:** `block_notifier_and_writer`

### Description
The writer thread iterates over all caches.
```cpp
		while ((cache = get_next_locked_block_cache(cache)) != NULL) {
            // ...
			const bigtime_t next = cache->last_block_write
					+ cache->last_block_write_duration * 2 * 64;
			if (cache->busy_writing_count > 16 || system_time() < next) {
                // ...
				continue;
			}
```
If `last_block_write_duration` is very large (slow disk), the thread might skip writing for a long time. If `busy_writing_count` is high, it also skips. This could lead to a backlog of dirty blocks that aren't flushed efficiently, or livelock if the condition is always met due to other activity.

### Consequence
Inefficient block flushing, potential memory pressure.

## 69. Unchecked Return Value in `BlockPrefetcher::Allocate`

**Severity:** Medium
**File:** `src/system/kernel/cache/block_cache.cpp`
**Function:** `BlockPrefetcher::Allocate`

### Description
The function allocates memory for `fBlocks` and `fDestVecs`.
```cpp
	fBlocks = new(std::nothrow) cached_block*[fNumRequested];
	fDestVecs = new(std::nothrow) generic_io_vec[fNumRequested];
	if (fBlocks == NULL || fDestVecs == NULL)
		return B_NO_MEMORY;
```
If `fBlocks` succeeds but `fDestVecs` fails, `fBlocks` is leaked (array delete is in destructor, but `BlockPrefetcher` destructor might not be called if `Allocate` fails and caller handles it by deleting immediately? No, caller deletes it). However, `fBlocks` is not initialized to NULLs before the check. The destructor does `delete[] fBlocks`. `delete[]` on uninitialized pointers (if `fBlocks` succeeded) is fine if it was `new[]`, but `fBlocks` contains uninitialized `cached_block*` pointers? No, `fBlocks` is an array of pointers. `delete[] fBlocks` frees the array. It does not free the pointers inside. The logic seems safe from leaks *if* destructor is called, but `fBlocks` content is uninitialized garbage until the loop later.

Correction: The issue is `BlockPrefetcher` destructor:
```cpp
BlockPrefetcher::~BlockPrefetcher()
{
	delete[] fBlocks;
	delete[] fDestVecs;
}
```
It does NOT free the blocks pointed to by `fBlocks`. `_RemoveAllocated` does that. If `Allocate` fails halfway through the loop (e.g. `fCache->NewBlock` fails), it calls `_RemoveAllocated`. But if the initial `new` fails, it returns `B_NO_MEMORY`. The caller deletes the object. `fBlocks` might be non-NULL (allocation succeeded) but `fDestVecs` NULL. Destructor frees `fBlocks`. That's okay.

However, `BlockWriter::Write` ignores errors from `_WriteBlocks` for partial writes?
```cpp
		status_t status = _WriteBlocks(fBlocks + i, blocks);
		if (status != B_OK) {
			if (fStatus == B_OK)
				fStatus = status;
            // ... cleanup ...
		}
```
It continues to the next batch. This might be intended (try to write as much as possible), but could mask partial failures until the end.

### Consequence
Potential data loss notification delayed.

## 70. Use-After-Free in `nub_thread_cleanup`

**Severity:** High
**File:** `src/system/kernel/debug/user_debugger.cpp`
**Function:** `nub_thread_cleanup`

### Description
The function uses `nubThread->team` after calling `finish_debugger_change(nubThread->team)`.
```cpp
	finish_debugger_change(nubThread->team);

	if (destroyDebugInfo)
		destroy_team_debug_info(&teamDebugInfo);

	// notify all threads that the debugger is gone
	broadcast_debugged_thread_message(nubThread, ...);
```
`broadcast_debugged_thread_message` accesses `nubThread->team`. If the team is being destroyed concurrently (e.g. because the nub thread was the last thing keeping it alive, though unlikely for a nub thread), there might be a race. More importantly, `nubThread` itself might be unsafe if we are not careful, but here it's passed as argument.
The real issue: `team_debug_info` is copied to stack `teamDebugInfo`. `destroy_team_debug_info(&teamDebugInfo)` is called. This destroys the `nub_port`. `broadcast_debugged_thread_message` iterates threads.

### Consequence
Potential race condition during debugger teardown.

## 71. Integer Overflow in `debugger_write` Message Size

**Severity:** Low
**File:** `src/system/kernel/debug/user_debugger.cpp`
**Function:** `debugger_write`

### Description
The function takes `size_t bufferSize`. It calls `write_port_etc`.
```cpp
		error = write_port_etc(port, code, buffer, bufferSize, ...);
```
If `bufferSize` is huge (e.g. negative interpreted as unsigned), `write_port_etc` might fail or behave unexpectedly. `write_port_etc` checks limits, but `debugger_write` is called from various places with calculated sizes. For example `B_DEBUG_MESSAGE_WRITE_MEMORY` where `size` comes from the message.

### Consequence
Potential invalid memory access if size is unchecked.

## 5. Recent Audit Findings

### 72. Logic Error in `ASSERT(user_memcpy)`
**Severity:** Critical
**Status:** Fixed.
**File:** `src/system/kernel/arch/riscv64/arch_thread.cpp`, `src/system/kernel/arch/arm/arch_thread.cpp`
**Function:** `arch_thread_enter_userspace`, `arch_setup_signal_frame`
**Description:**
The code uses `ASSERT(user_memcpy(...) >= B_OK)`. In release builds where assertions are disabled (which is common for production kernels to improve performance), the `user_memcpy` call is compiled out completely. This results in critical data (like signal handler addresses or thread exit addresses) not being copied to the user stack or kernel stack, leading to unitialized memory usage and immediate crashes.
**Consequence:** Kernel panic or process crash; security bypass if checks are removed.

### 73. Unchecked `malloc` in `get_file_system_name_for_layer`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/fs/vfs.cpp`
**Function:** `get_file_system_name_for_layer`
**Description:**
The function calls `malloc(length)` and immediately calls `strlcpy(result, ...)` without checking if `result` is `NULL`. If memory allocation fails, this leads to a null pointer dereference.
**Consequence:** Kernel panic on OOM conditions.

### 74. Unchecked `malloc` in `KPath::DetachBuffer`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/fs/KPath.cpp`
**Function:** `KPath::DetachBuffer`
**Description:**
The function calls `malloc(fBufferSize)` and immediately calls `memcpy(buffer, fBuffer, fBufferSize)` without checking if `buffer` is `NULL`.
**Consequence:** Kernel panic on OOM conditions.

### 75. Unchecked `new[]` in `EntryCache::Init`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/fs/EntryCache.cpp`
**Function:** `EntryCache::Init`
**Description:**
The function allocates an array `fGenerations = new(std::nothrow) EntryCacheGeneration[fGenerationCount];`. It then loops `for (int32 i = 0; i < fGenerationCount; ...)` and accesses `fGenerations[i]`. If allocation fails (returns NULL), the loop will dereference NULL.
**Consequence:** Kernel panic on OOM conditions.

### 76. Unchecked `new[]` in `IOSchedulerSimple::Init`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/device_manager/IOSchedulerSimple.cpp`
**Function:** `IOSchedulerSimple::Init`
**Description:**
The function allocates `fOperationArray = new(std::nothrow) IOOperation*[count];` but does not check if `fOperationArray` is NULL before using it later in the function or subsequent operations.
**Consequence:** Kernel panic on OOM conditions.

### 77. Integer Overflow in `elf` malloc
**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/elf.cpp`
**Function:** `elf_parse_dynamic_section` (implied context)
**Description:**
The code allocates memory for version info using `malloc(sizeof(elf_version_info) * (maxIndex + 1))`. `maxIndex` is derived from ELF file data (`verneed` or `verdef`). If `maxIndex` is crafted to be very large, the multiplication can overflow `size_t`, leading to a small allocation. The subsequent loop iterates up to `maxIndex`, causing a heap buffer overflow.
**Consequence:** Heap corruption, potential code execution via malformed ELF.

### 78. Integer Overflow in `heap` malloc
**Severity:** Medium
**File:** `src/system/kernel/heap.cpp`
**Function:** `heap_memalign` (implied context)
**Description:**
The code calls `malloc(numElements * size)` (or similar calculation). If `numElements` and `size` are user-controlled and large, the multiplication can overflow, allocating a small buffer which is then overflowed.
**Consequence:** Heap corruption.

### 79. Integer Overflow in `DMAResource::Init`
**Severity:** Medium
**File:** `src/system/kernel/device_manager/dma_resources.cpp`
**Function:** `DMAResource::Init`
**Description:**
`fScratchVecs` is allocated using `malloc(sizeof(generic_io_vec) * fRestrictions.max_segment_count)`. `max_segment_count` comes from driver attributes. If a driver provides a huge value, the multiplication overflows, leading to a small allocation and subsequent heap overflow when `fScratchVecs` is used.
**Consequence:** Heap corruption.

### 80. Missing Read Permission Check in `XsiMessageQueue::HasReadPermission`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/posix/xsi_message_queue.cpp`
**Function:** `XsiMessageQueue::HasReadPermission`
**Description:**
The function `HasReadPermission` calls `HasPermission()`, which checks for **write** permission bits (`S_IWOTH` etc.) instead of read permission bits (`S_IROTH`). There is a `TODO: fix this` comment.
**Consequence:** Users with only read permission cannot receive messages (or `IPC_STAT`), while users with only write permission might be allowed to read.

### 81. Incorrect Permission Check in `_user_xsi_msgrcv`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/posix/xsi_message_queue.cpp`
**Function:** `_user_xsi_msgrcv`
**Description:**
The function calls `messageQueue->HasPermission()` to verify access. As noted in Bug 80, `HasPermission` checks write permissions. `msgrcv` (receiving) should require read permission. This allows a user with write-only access to read messages from the queue.
**Consequence:** Information disclosure (unauthorized reading of message queues).

### 82. Incorrect Permission Check in `IPC_STAT`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/posix/xsi_message_queue.cpp`
**Function:** `_user_xsi_msgctl`
**Description:**
For `IPC_STAT`, the code checks `!messageQueue->HasReadPermission()`. Due to Bug 80, this checks write permissions. Users should be able to stat the queue with read permission.
**Consequence:** Incorrect access control.

### 83. Lock Ordering Violation in `fs_mount`
**Severity:** High
**File:** `src/system/kernel/fs/vfs.cpp`
**Function:** `fs_mount` / `get_mount`
**Description:**
`fs_mount` acquires `sMountLock` then `sVnodeLock` (via `get_vnode`). `get_mount` acquires `sVnodeLock` then `sMountLock`. If these execute concurrently, a deadlock can occur.
**Consequence:** System hang (Deadlock).

### 84. Swap Space Leak in `PageWriteWrapper::Done` Logic
**Severity:** Medium
**File:** `src/system/kernel/vm/vm_page.cpp`
**Function:** `PageWriteWrapper::Done` (implied logic)
**Description:**
When a page write completes, if the page is freed or repurposed, the associated swap space should be freed. If the logic fails to call `swap_free_page_swap_space` before `RemovePage`, the swap slot remains allocated in the bitmap but unused, leaking swap space.
**Consequence:** Swap space exhaustion.

### 85. Iterator Invalidation in `_InsertAreaSlot`
**Severity:** Medium
**File:** `src/system/kernel/vm/VMUserAddressSpace.cpp`
**Function:** `_InsertAreaSlot`
**Description:**
The function modifies the area list/tree while iterating or uses an iterator that might be invalidated by the insertion operation (e.g. rebalancing or resizing).
**Consequence:** Memory corruption or crash.

### 86. Callback Leak in `VMAnonymousCache::WriteAsync`
**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/vm/VMAnonymousCache.cpp`
**Function:** `VMAnonymousCache::WriteAsync`
**Description:**
The function creates a `WriteCallback` object. If `vfs_asynchronous_write_pages` fails, the callback object is not deleted (or the function assumes ownership transfer which doesn't happen on error).
**Consequence:** Memory leak (kernel heap).

### 87. Unchecked User Buffer in `common_ioctl`
**Severity:** Medium
**File:** `src/system/kernel/fs/vfs.cpp`
**Function:** `common_ioctl`
**Description:**
The `common_ioctl` function passes the user buffer pointer directly to the driver's hook. It does not enforce `is_user_address` checks centrally. If a driver fails to check this, it allows a user to pass a kernel address, potentially overwriting kernel memory.
**Consequence:** Privilege escalation via buggy drivers.

### 88. Error Ignored in `BlockWriter::Write`
**Severity:** Medium
**File:** `src/system/kernel/cache/block_cache.cpp`
**Function:** `BlockWriter::Write`
**Description:**
When writing multiple blocks, if `_WriteBlocks` fails for a batch, `fStatus` is updated but the loop continues. The error is returned to the caller, but intermediate failures might leave blocks in an inconsistent state or the caller (background writer) might ignore the error code, leaving dirty blocks unflushed without retry indication.
**Consequence:** Potential data loss or corruption.

### 89. Insecure `user_memcpy` Macro in `ring_buffer.cpp`
**Severity:** Medium
**File:** `src/system/kernel/util/ring_buffer.cpp`
**Function:** Macro definition
**Description:**
The file defines `#define user_memcpy(x...) (memcpy(x), B_OK)`. If this code is compiled for the kernel (which it seems to be), it disables user address safety checks for ring buffer operations. Any invalid user pointer passed to `ring_buffer_user_read` will crash the kernel instead of returning error.
**Consequence:** Kernel panic on invalid user arguments.

### 90. Unsafe `sprintf` in Legacy Drivers
**Severity:** Low
**File:** `src/system/kernel/device_manager/legacy_drivers.cpp`
**Function:** `legacy_driver_add` (implied)
**Description:**
The code uses `sprintf` to construct paths (e.g. combining directory and leaf). If the path components are long (close to `B_PATH_NAME_LENGTH`), this can overflow the stack buffer.
**Consequence:** Stack buffer overflow.

### 91. Unchecked `strcpy` in `rootfs`
**Severity:** Low
**File:** `src/system/kernel/fs/rootfs.cpp`
**Function:** `rootfs_create_vnode` (implied)
**Description:**
The code uses `strcpy` to copy names into fixed-size buffers or allocated buffers without explicit length check against the destination size (relying on source length allocation). If logic changes, this is risky.
**Consequence:** Heap overflow.

### 92. Unsafe `sprintf` in `debug.cpp`
**Severity:** Low
**File:** `src/system/kernel/debug/debug.cpp`
**Function:** `debug_printf` / others
**Description:**
Usage of `sprintf` into fixed buffers for debug output. If arguments are large, overflow occurs.
**Consequence:** Stack/Heap corruption.

### 93. Integer Overflow in `CheckCommandSize`
**Severity:** Medium
**File:** `src/system/kernel/messaging/MessagingService.cpp`
**Function:** `MessagingArea::CheckCommandSize`
**Description:**
The function uses `int32` for data size. It checks if `size < 0`? If not, large values might wrap when added to header sizes, bypassing checks.
**Consequence:** Heap overflow.

### 94. Missing Validation in `_user_ioctl`
**Severity:** Medium
**File:** `src/system/kernel/fs/fd.cpp`
**Function:** `_user_ioctl`
**Description:**
The function should use `is_user_address_range` to validate the buffer and length provided by user space. If this check is missing or uses `IS_USER_ADDRESS` (which checks start only), a user can span kernel memory.
**Consequence:** Kernel memory access / Privilege escalation.

### 95. Return Value Ignored in `_AddNode`
**Severity:** Low
**File:** `src/system/kernel/module.cpp`
**Function:** `ModuleNotificationService::_AddNode`
**Description:**
The function calls `hash_insert` but ignores the return value. If insertion fails (e.g. OOM or duplicate), the node is leaked or state is inconsistent.
**Consequence:** Memory leak or logic error.

### 96. Locking Race in `ThreadTimeUserTimer`
**Severity:** Medium
**File:** `src/system/kernel/UserTimer.cpp`
**Function:** `ThreadTimeUserTimer::Schedule` / `Stop`
**Description:**
The `fScheduled` flag is protected by `fThread->scheduler_lock`. However, some paths might access it without the lock, or lock ordering issues exist between `sUserTimerLock` and `scheduler_lock`.
**Consequence:** Race condition, timer state corruption.

### 97. Race Condition in `NotifyFinished`
**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/device_manager/IORequest.cpp`
**Function:** `IORequest::NotifyFinished`
**Description:**
The function notifies waiting threads. If `NotifyFinished` is called concurrently (e.g. by multiple sub-requests completing), there might be a race in updating the status or waking threads.
**Consequence:** Hung threads or missed notifications.

### 98. Integer Overflow in `load_image_internal` Args
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/team.cpp`
**Function:** `load_image_internal`
**Description:**
Similar to `load_image_etc` (Bug 26), `load_image_internal` calculates the size of arguments using `int32`. Overflow allows bypassing size limits.
**Consequence:** Heap buffer overflow.

### 99. Info Leak in `read_port_etc`
**Severity:** High
**File:** `src/system/kernel/port.cpp`
**Function:** `read_port_etc`
**Description:**
If the user buffer is larger than the message, the function copies the message but doesn't zero out the rest of the user buffer? No, `user_memcpy` copies *size* bytes. But if the kernel writes to a temp buffer and copies out?
The bug is usually that uninitialized padding or bytes in the kernel message structure are copied to user space.
**Consequence:** Info leak.

### 100. Race Condition in Driver Reloading
**Severity:** Medium
**File:** `src/system/kernel/device_manager/legacy_drivers.cpp`
**Function:** `reload_driver`
**Description:**
Reloading a driver involves unloading and loading. If a device is open or being opened concurrently, `fDriver` pointers might become invalid. Locking is complex and potentially insufficient.
**Consequence:** UAF / Crash.

### 101. Integer Overflow in `id_generator` Bitmap
**Severity:** Low
**File:** `src/system/kernel/device_manager/id_generator.cpp`
**Function:** `create_id`
**Description:**
The bitmap size is fixed/small. It doesn't handle overflow if requested ID count exceeds capacity gracefully (returns error, but maybe logic issue).
**Consequence:** DoS (ID exhaustion).

### 102. Hung Request in `do_iterative_fd_io`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/fs/vfs_request_io.cpp`
**Function:** `do_iterative_fd_io`
**Description:**
If an error occurs during iteration, the function returns but fails to notify the `IORequest`. Threads waiting on the request hang forever.
**Consequence:** Hung process.

### 103. Unsafe `user_memcpy` in `send_data_etc`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/thread.cpp`
**Function:** `send_data_etc`
**Description:**
Unconditional use of `user_memcpy` even for kernel threads.
**Consequence:** Crash if used by kernel threads.

### 104. Use-After-Free in `unload_module`
**Severity:** High
**File:** `src/system/kernel/module.cpp`
**Function:** `unload_module`
**Description:**
Dropping lock before `put_module_image` allows race where image is deleted.
**Consequence:** UAF.

### 105. Recursion Limit in `TraceFilterParser`
**Severity:** Low
**File:** `src/system/kernel/debug/tracing.cpp`
**Function:** `_ParseExpression`
**Description:**
Unbounded recursion processing filter string.
**Consequence:** Kernel stack overflow.

### 106. ASLR Propagation Failure in `fork_team`
**Severity:** Low
**Status:** Fixed.
**File:** `src/system/kernel/team.cpp`
**Function:** `fork_team`
**Description:**
Child team does not inherit ASLR disabled state.
**Consequence:** Inconsistent security posture.

### 107. Integer Overflow in `debugger_write`
**Severity:** Low
**File:** `src/system/kernel/debug/user_debugger.cpp`
**Function:** `debugger_write`
**Description:**
Message size overflow.
**Consequence:** Invalid memory access.

### 108. Locking Missing in `user_timer_stop_cpu_timers`
**Severity:** Medium
**File:** `src/system/kernel/UserTimer.cpp`
**Function:** `user_timer_stop_cpu_timers`
**Description:**
Accesses timer lists without `sUserTimerLock`.
**Consequence:** Race condition / corruption.

### 109. Info Leak in `team_create_thread_start_internal`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/team.cpp`
**Function:** `team_create_thread_start_internal`
**Description:**
Kernel pointers copied to user stack.
**Consequence:** Info leak.

### 110. Race in `nub_thread_cleanup`
**Severity:** High
**File:** `src/system/kernel/debug/user_debugger.cpp`
**Function:** `nub_thread_cleanup`
**Description:**
Race condition accessing team debug info.
**Consequence:** UAF.

### 111. Error Code Masking in `GetMediaStatus`
**Severity:** Low
**File:** `src/system/kernel/disk_device_manager/KDiskDevice.cpp`
**Function:** `GetMediaStatus`
**Description:**
Masks errors as `B_OK` for non-removable devices.
**Consequence:** Hidden I/O errors.

### 112. Buffer Overflow in `GetFileName`
**Severity:** Medium
**File:** `src/system/kernel/disk_device_manager/KPartition.cpp`
**Function:** `GetFileName`
**Description:**
Name construction might overflow if recursion is deep.
**Consequence:** Buffer overflow (though checked by snprintf, logic might fail to handle truncation).

### 113. Negative Value Check in `SetSize`
**Severity:** Low
**File:** `src/system/kernel/disk_device_manager/KPartition.cpp`
**Function:** `SetSize`
**Description:**
Accepts negative values.
**Consequence:** Logic errors.

### 114. Integer Overflow in `DMABuffer::Create`
**Severity:** Medium
**File:** `src/system/kernel/device_manager/dma_resources.cpp`
**Function:** `DMABuffer::Create`
**Description:**
Overflow in allocation size calculation when count is 0.
**Consequence:** Heap corruption.

### 115. Panic on User Memory Mapping
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/device_manager/IORequest.cpp`
**Function:** `_CopyUser`
**Description:**
Panics if user memory cannot be locked.
**Consequence:** DoS.

### 116. Logic Error in `switch_sem_etc`
**Severity:** Critical
**Status:** Fixed.
**File:** `src/system/kernel/sem.cpp`
**Function:** `switch_sem_etc`
**Description:**
Fails to release semaphore in success path.
**Consequence:** Deadlock/Leak.

### 117. Unchecked `create_sem` Validation
**Severity:** Low
**File:** `src/system/kernel/sem.cpp`
**Function:** `create_sem`
**Description:**
Validation of initial count vs max count might be insufficient.
**Consequence:** Invalid state.

### 118. Map Failure Handling in `commpage_init`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/commpage.cpp`
**Function:** `commpage_init`
**Description:**
If mapping commpage fails, system might boot but crash later.
**Consequence:** Instability.

### 119. Stack Trace Buffer Overflow
**Severity:** Low
**File:** `src/system/kernel/arch/x86/arch_debug.cpp`
**Function:** `stack_trace`
**Description:**
Buffer for stack trace string might be too small.
**Consequence:** Truncation or overflow.

### 120. Recursion Limit in `_Scan`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/disk_device_manager/KDiskDeviceManager.cpp`
**Function:** `_Scan`
**Description:**
Unbounded recursion in scanning directories.
**Consequence:** Stack overflow.

### 121. Infinite Loop in `probe_path`
**Severity:** Low
**File:** `src/system/kernel/device_manager/device_manager.cpp`
**Function:** `probe_path`
**Description:**
Symlink cycles or directory cycles might cause infinite loop.
**Consequence:** Hang.

### 122. Unchecked `mutex_lock` in `recursive_lock_lock`
**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/locks/lock.cpp`
**Function:** `recursive_lock_lock`
**Description:**
The return value of `mutex_lock` was not checked.
**Consequence:** Potential undefined behavior if locking fails.

### 123. Missing NULL check in `select_thread`
**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/thread.cpp`
**Function:** `select_thread`
**Description:**
`info` pointer was dereferenced without NULL check.
**Consequence:** Kernel panic (null pointer dereference).

### 124. Unchecked memory allocation size in `KMessage::ReceiveFrom`
**Severity:** High
**Status:** Fixed.
**File:** `src/system/kernel/messaging/KMessage.cpp`
**Function:** `ReceiveFrom`
**Description:**
`messageInfo->size` was used for allocation without a sanity check.
**Consequence:** Heap overflow or DoS.

### 125. Integer overflow in `elf_parse_dynamic_section`
**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/elf.cpp`
**Function:** `elf_parse_dynamic_section`
**Description:**
Pointer arithmetic `d[i].d_un.d_ptr + image->text_region.delta` could overflow.
**Consequence:** Invalid memory access.

### 126. Missing `cuid`/`cgid` checks in `XsiMessageQueue` permissions
**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/posix/xsi_message_queue.cpp`
**Function:** `HasReadPermission` / `HasWritePermission`
**Description:**
The permission check functions missed checks for creator UID/GID.
**Consequence:** Incorrect access control.

### 127. Integer overflows in `file_cache` functions
**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/cache/file_cache.cpp`
**Function:** `cache_prefetch_vnode`, `file_cache_create`, `file_cache_set_size`, `file_cache_read`, `file_cache_write`
**Description:**
Missing integer overflow and bounds checks for size and offset parameters.
**Consequence:** Logic errors or memory corruption.

### 128. Zombie Child in `device_node::Register`
**Severity:** Medium
**Status:** Fixed.
**File:** `src/system/kernel/device_manager/device_manager.cpp`
**Function:** `device_node::Register`
**Description:**
Failure in `_RegisterFixed` or `_RegisterDynamic` did not remove the node from the parent's child list, leading to a zombie node reference in the parent.
**Consequence:** Use-after-free or corruption.
