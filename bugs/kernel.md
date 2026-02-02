# Kernel Bugs

This document lists coding errors and bugs identified in `src/system/kernel`.

## 1. Off-by-one Overflow Check in `inet_ntop.c`

**File:** `src/system/kernel/util/inet_ntop.c`

**Issue:**
The overflow check in `inet_ntop6` uses `> size` instead of `>= size`. If the formatted string length equals the buffer size, the check passes, but `strcpy` writes `size + 1` bytes (including the null terminator), causing a one-byte buffer overflow.

**Code:**
```c
	/*
	 * Check for overflow, copy, and we're done.
	 */
	if ((size_t)(tp - tmp) > size) {
		errno = ENOSPC;
		return (NULL);
	}
	strcpy(dst, tmp);
	return (dst);
```

**Fix:**
Change the condition to `>= size` to account for the null terminator.

---

## 2. Incorrect Buffer Size in `user_strlcpy` (devfs)

**File:** `src/system/kernel/device_manager/devfs.cpp`

**Issue:**
In the `B_GET_PATH_FOR_DEVICE` ioctl handler, `user_strlcpy` is called with `sizeof(path)` (the size of the source kernel buffer, 256 bytes) as the destination size, instead of using the user-provided buffer length (`length`). This can lead to a user-space buffer overflow if the user's buffer is smaller than 256 bytes but the path is long (but fits in 256 bytes).

**Code:**
```cpp
			case B_GET_PATH_FOR_DEVICE:
			{
				char path[256];
				// ...
				strcpy(path, "/dev/");
				get_device_name(vnode, path + 5, sizeof(path) - 5);
				if (length && (length <= strlen(path)))
					return ERANGE;
				return user_strlcpy((char*)buffer, path, sizeof(path));
			}
```

**Fix:**
Use `length` (if non-zero) as the size argument for `user_strlcpy`.

---

## 3. Unsafe `strcpy` in Debugger Line Editing

**File:** `src/system/kernel/debug/debug.cpp`

**Issue:**
In `read_line`, the history recall feature (Up/Down arrow) uses `strcpy` to copy a history line (`sLineBuffer[historyLine]`, size 1024) into the current line buffer (`buffer`). If `read_line` is called with a smaller buffer (e.g. via `kgets`), this `strcpy` can overflow the destination buffer.

**Code:**
```cpp
						// swap the current line with something from the history
						if (position > 0)
							kprintf("\x1b[%" B_PRId32 "D", position); // move to beginning of line

						strcpy(buffer, sLineBuffer[historyLine]);
						length = position = strlen(buffer);
```

**Fix:**
Use `strlcpy(buffer, sLineBuffer[historyLine], maxLength)` to prevent overflow.

---

## 4. Unsafe `vsprintf` in GDB Stub

**File:** `src/system/kernel/debug/gdb.cpp`

**Issue:**
The `gdb_reply` function uses `vsprintf` to format a message into a fixed-size buffer `sReply` (512 bytes). If the formatted message exceeds this size, a kernel stack/data overflow occurs.

**Code:**
```cpp
static void
gdb_reply(char const* format, ...)
{
	// ...
	va_start(args, format);
	sReply[0] = '$';
	vsprintf(sReply + 1, format, args);
	va_end(args);
	// ...
}
```

**Fix:**
Use `vsnprintf` to limit the output to the buffer size.
