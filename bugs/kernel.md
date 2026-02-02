# Kernel Bugs

## 1. Mutex Destruction Race Condition
In `src/system/kernel/locks/lock.cpp`, there is a documented race condition during mutex destruction. If a thread is waiting on a mutex with a timeout and the timeout occurs at the same time the mutex is being destroyed, a race condition can occur if the thread resumes before its `thread` entry in the waiter structure is set to `NULL`.

## 2. Team ID Wrap-around Bug
In `src/system/kernel/team.cpp`, the logic for iterating through teams (e.g., in `get_next_team_info`) is potentially broken when team IDs wrap around. It uses `peek_next_thread_id()` to determine the upper bound for iteration, but if IDs have wrapped and are being reused, this may not correctly cover all active teams.

## 3. Missing Deadlock Detection in Advisory Locking
In `src/system/kernel/fs/vfs.cpp`, the `acquire_advisory_lock` function lacks deadlock detection. This could allow multiple processes to enter a deadlock state when competing for file locks, which the kernel currently does not detect or resolve.

## 4. Unreliable Semaphore Iteration
In `src/system/kernel/sem.cpp`, the function `sem_get_next_sem_info` uses an index-based approach to iterate through a team's semaphore list. Since the list can be modified between calls, this method of iteration is unreliable and can skip semaphores or return incorrect information.

## 5. High Stack Usage in File Cache
In `src/system/kernel/cache/file_cache.cpp`, the `read_into_cache` function allocates large arrays for I/O vectors (`generic_io_vec vecs[MAX_IO_VECS]`) on the stack. In the kernel, stack space is limited, and this could potentially lead to stack overflow. These should be allocated on the heap instead.

## 6. Fixed-size I/O Vectors in Page Writer
In `src/system/kernel/vm/vm_page.cpp`, the `PageWriterRun` class uses a fixed-size array for I/O vectors (`generic_io_vec fVecs[32]`). This limits the number of non-contiguous pages that can be written in a single I/O operation, potentially reducing I/O efficiency.
