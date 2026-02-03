# Filesystem Code Audit Findings

## 1. Critical: Stack Buffer Overflow in FAT File System (Fixed)

**Location:** `src/add-ons/kernel/file_systems/fat/encodings.cpp`
**Functions:** `munge_short_name_english`, `munge_short_name_sjis`

**Description:**
The functions `munge_short_name_english` and `munge_short_name_sjis` use `sprintf` to format a 64-bit integer (`value`) into an 8-byte buffer (`char buffer[8]`). The format string used is `"~%" B_PRIu64`.

If `iteration` exceeds 9,999,999, the resulting string (e.g., `~10000000`) requires 9 bytes + null terminator, overflowing the 8-byte stack buffer. This can occur if a directory contains a very large number of files with similar names, leading to a high collision count during short name generation.

**Fix Applied:**
Increased buffer size to 24 bytes and replaced `sprintf` with `snprintf`.

## 2. High: Integer Overflow in BFS Block Allocator (Fixed)

**Location:** `src/add-ons/kernel/file_systems/bfs/BlockAllocator.cpp`
**Function:** `BlockAllocator::InitializeAndClearBitmap`

**Description:**
The size of the block bitmap buffer is calculated using 32-bit arithmetic, potentially leading to an overflow if the allocation group size is large.

```cpp
	uint32 numBits = 8 * fBlocksPerGroup * fVolume->BlockSize();
	uint32 blockShift = fVolume->BlockShift();

	uint32* buffer = (uint32*)malloc(numBits >> 3);
```

**Fix Applied:**
Used `uint64` and `size_t` for intermediate size calculations and added checks to prevent overflow.

## 3. Medium: Unchecked Buffer Size in FAT `dosfs_read_attrdir` (Fixed)

**Location:** `src/add-ons/kernel/file_systems/fat/kernel_interface.cpp`
**Function:** `dosfs_read_attrdir`

**Description:**
The function `strcpy`s a constant string into the dirent buffer without checking if the buffer size provided by the VFS is sufficient.

**Fix Applied:**
Added a check for `bufferSize` against the required length before writing to `buffer`.

## 4. General Improvements

*   **Unsafe String Functions:** Multiple instances of `strcpy` and `sprintf` were found in `bfs` and `fat`. While many appeared safe due to context, replacing them with `strlcpy` and `snprintf` is strongly recommended for defense-in-depth.
*   **Integer Arithmetic:** File systems often handle data structures defined by disk formats. Care must be taken to cast to `uint64` before multiplying or shifting 32-bit values read from disk to avoid wrapping issues on large volumes.
