# POSIX Implementation Assessment

This document outlines missing POSIX features and areas for improvement in the Haiku codebase, ranked by difficulty and importance.

## Missing Features

### 1. Open File Description Locks (OFD)
*   **Missing Macro:** `F_OFD_SETLK` (and `F_OFD_SETLKW`, `F_OFD_GETLK`).
*   **Description:** Advisory byte-range locks associated with the open file description, rather than the process. Useful for multi-threaded applications to avoid accidental unlocking by other threads.
*   **Difficulty:** **Medium**. Requires kernel VFS changes to track locks per file description rather than per process-file pair.
*   **Importance:** **Medium**. increasingly used by modern database libraries (e.g., SQLite) and multi-threaded tools.

### 2. Asynchronous I/O (AIO)
*   **Missing Header:** `<aio.h>`
*   **Description:** Functions for asynchronous read/write operations (`aio_read`, `aio_write`, `lio_listio`, etc.).
*   **Difficulty:** **High**. Requires significant kernel support to be truly asynchronous and efficient.
*   **Importance:** **Medium**. Critical for high-performance servers and databases (e.g., PostgreSQL, Nginx).

### 3. POSIX Message Queues
*   **Missing Header:** `<mqueue.h>`
*   **Description:** Named message queues (`mq_open`, `mq_send`, `mq_receive`).
*   **Status:** XSI (System V) message queues are present, but POSIX ones are missing. `os-test` gives a score of 8% for `MSG`.
*   **Difficulty:** **Medium**. Requires kernel IPC implementation and user-space library support.
*   **Importance:** **Medium**. Used in real-time applications and modern IPC designs.

### 4. User Context Switching
*   **Missing/Incomplete:** `<ucontext.h>` functions (`getcontext`, `setcontext`, `makecontext`, `swapcontext`).
*   **Description:** Mechanism for user-level context switching.
*   **Difficulty:** **High**. Architecture-dependent assembly implementation required for each supported platform (x86, x86_64, ARM, etc.).
*   **Importance:** **High**. Essential for language runtimes (Go), coroutine libraries, and some emulators.

### 5. Robust Mutexes
*   **Missing Feature:** `PTHREAD_MUTEX_ROBUST`.
*   **Description:** Allows recovering a mutex if the owner dies while holding it.
*   **Status:** Macros `_POSIX_THREAD_ROBUST_PRIO_INHERIT` and `_POSIX_THREAD_ROBUST_PRIO_PROTECT` are defined as `-1` (unsupported) in `unistd.h`.
*   **Difficulty:** **Medium**. Requires kernel support to detect thread death and mark the mutex state.
*   **Importance:** **Medium**. Important for reliable shared-memory applications and databases.

### 6. Process Tracing
*   **Missing Header:** `<trace.h>`
*   **Description:** POSIX Trace standard.
*   **Difficulty:** **High**.
*   **Importance:** **Low**. Most systems use platform-specific tracing (ptrace, DTrace) or debuggers.

### 7. STREAMS
*   **Missing Header:** `<stropts.h>`
*   **Description:** STREAMS interface.
*   **Difficulty:** **High**.
*   **Importance:** **Low**. Obsolescent and rarely used in modern software.

### 8. XSI Shared Memory (Headers)
*   **Missing Header:** `<sys/shm.h>`
*   **Description:** System V style shared memory headers. POSIX shared memory (`shm_open`) is implemented.
*   **Status:** `os-test` indicates incomplete support (62%) for SHM.
*   **Difficulty:** **Medium**.
*   **Importance:** **Medium**. Required for legacy applications.

## Compliance Verification (os-test)

Based on results from the [os-test](https://sortix.org/os-test/) suite, Haiku has an overall compliance score of approximately **88%**.

### Areas needing improvement:
*   **I/O (`io` suite):** **40%**. Significant gaps in input/output system calls.
*   **Limits (`limits` suite):** **35%**. `limits.h` constants may be incorrect or missing.
*   **Namespace (`namespace` suite):** **20%**. Significant namespace pollution in standard headers.
*   **Process (`process` suite):** **65%**. Missing or non-compliant process system calls.
*   **Terminal (`pty` suite):** **62%**. Pseudoterminal support issues.
*   **Process Scheduling (`PS` option):** **20%**. `sched.h` and related functionality are largely unimplemented or non-compliant.

### Verified Implemented Features:
The following features are implemented in the codebase, though corner cases may exist:
*   `pthread_cancel`: Implemented (supports asynchronous cancellation).
*   `pthread_cond_init`: Implemented (supports `CLOCK_MONOTONIC`).
*   `pthread_cond_timedwait`: Implemented (respects clock attribute).
*   `pthread_detach`: Implemented.
*   `pthread_rwlock_*`: Implemented (Read/Write/Timed/Try variants present).

## Improvements & Partial Implementations

### 1. Thread Scheduling Attributes
*   **Issue:** `TPS` (Thread Execution Scheduling) scores **64%** in `os-test`. Functions like `pthread_attr_setinheritsched` are declared but often return errors or are stubs.
*   **Difficulty:** **Medium**. Requires binding pthread attributes to kernel scheduler parameters.
*   **Importance:** **Medium**. Crucial for real-time compliance.

### 2. Complex Math
*   **Issue:** `<complex.h>` and `<tgmath.h>` support should be verified against C99/C11 compliance.
*   **Difficulty:** **Low**.
*   **Importance:** **Medium**. Scientific computing.

### 3. Word Expansion
*   **Missing Function:** `wordexp()` from `<wordexp.h>`.
*   **Description:** Shell-like word expansion.
*   **Difficulty:** **Low/Medium**. Can be ported from musl/glibc.
*   **Importance:** **Low**. Used by CLI utilities.
