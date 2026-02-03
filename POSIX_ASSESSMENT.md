# POSIX Implementation Assessment

This document outlines missing POSIX features and areas for improvement in the Haiku codebase, ranked by difficulty and importance.

## Missing Features

### 1. Asynchronous I/O (AIO)
*   **Missing Header:** `<aio.h>`
*   **Description:** Functions for asynchronous read/write operations (`aio_read`, `aio_write`, `lio_listio`, etc.).
*   **Difficulty:** **High**. Requires significant kernel support to be truly asynchronous and efficient.
*   **Importance:** **Medium**. Critical for high-performance servers and databases (e.g., PostgreSQL, Nginx).

### 2. POSIX Message Queues
*   **Missing Header:** `<mqueue.h>`
*   **Description:** Named message queues (`mq_open`, `mq_send`, `mq_receive`).
*   **Status:** XSI (System V) message queues are present, but POSIX ones are missing.
*   **Difficulty:** **Medium**. Requires kernel IPC implementation and user-space library support.
*   **Importance:** **Medium**. Used in real-time applications and modern IPC designs.

### 3. User Context Switching
*   **Missing/Incomplete:** `<ucontext.h>` functions (`getcontext`, `setcontext`, `makecontext`, `swapcontext`).
*   **Description:** Mechanism for user-level context switching.
*   **Difficulty:** **High**. Architecture-dependent assembly implementation required for each supported platform (x86, x86_64, ARM, etc.).
*   **Importance:** **High**. Essential for language runtimes (Go), coroutine libraries, and some emulators.

### 4. Robust Mutexes
*   **Missing Feature:** `PTHREAD_MUTEX_ROBUST` protocol.
*   **Description:** Allows recovering a mutex if the owner dies while holding it.
*   **Difficulty:** **Medium**. Requires kernel support to detect thread death and mark the mutex state.
*   **Importance:** **Medium**. Important for reliable shared-memory applications and databases.

### 5. Word Expansion
*   **Missing Function:** `wordexp()` from `<wordexp.h>`.
*   **Description:** Shell-like word expansion.
*   **Difficulty:** **Low/Medium**. Can likely be ported from other open-source libc implementations (e.g., musl, glibc).
*   **Importance:** **Low**. Mainly used by CLI utilities.

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

### 8. XSI Shared Memory
*   **Missing Header:** `<sys/shm.h>`
*   **Description:** System V style shared memory (`shmget`, `shmat`).
*   **Status:** POSIX shared memory (`shm_open`) is implemented.
*   **Difficulty:** **Medium**.
*   **Importance:** **Medium**. Required for legacy applications.

## Improvements & Partial Implementations

### 1. Thread Scheduling Attributes
*   **Issue:** Scheduling attribute functions in `<pthread.h>` are declared but commented out or unimplemented (`pthread_attr_setinheritsched`, `pthread_attr_setschedpolicy`).
*   **Difficulty:** **Medium**. Requires binding pthread attributes to kernel scheduler parameters.
*   **Importance:** **Medium**. Crucial for real-time compliance.

### 2. Complex Math
*   **Issue:** `<complex.h>` and `<tgmath.h>` support should be verified against C99/C11 compliance.
*   **Difficulty:** **Low**.
*   **Importance:** **Medium**. Scientific computing.

### 3. Localization
*   **Issue:** `<iconv.h>` is located in `headers/libs/iconv/` rather than the standard path, potentially complicating porting.
*   **Difficulty:** **Low**.
*   **Importance:** **High**. Internationalization support.
