# Kernel Audit Report

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
