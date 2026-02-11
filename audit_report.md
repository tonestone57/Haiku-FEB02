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
