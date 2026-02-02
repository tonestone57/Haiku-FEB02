# Installer Bugs

## 1. Use-after-free and Race Condition in `CopyEngine`
In `src/apps/installer/CopyEngine.cpp`, the `_CopyData` method allocates a `BFile` object for the destination. This pointer is shared with multiple `Buffer` objects that are pushed into a `fBufferQueue` to be processed by a separate writer thread.
If an error occurs during the reading loop (e.g., `read < 0` or `fBufferQueue.Push` fails), the `destination` file is deleted immediately in the main thread. However, there may be `Buffer` objects already in the queue that still hold a pointer to this `destination` file. When the writer thread later attempts to use these buffers, it will access a deleted `BFile` object, leading to a crash or undefined behavior.

## 2. Buffer Overflow in `make_partition_label`
In `src/apps/installer/WorkerThread.cpp`, the `make_partition_label` function uses `sprintf` to write to buffers of size 255. Since partition names and paths can be much longer (up to `B_PATH_NAME_LENGTH`, which is 1024), this can easily cause a buffer overflow if a partition has a long name or a deeply nested mount point.

## 3. Filenames with Spaces Bug in `UnzipEngine`
In `src/apps/installer/UnzipEngine.cpp`, the `_ReadLineListing` method uses `sscanf` with a format that includes `%s` for the file path. This causes parsing to fail for filenames that contain spaces, as `sscanf` will stop at the first space. A more robust parsing method using offsets should be used.
