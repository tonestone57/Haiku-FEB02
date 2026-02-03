# POSIX Implementation Assessment

This document outlines missing POSIX features and areas for improvement in the Haiku codebase, ranked by difficulty and importance.

## Modern Software Relevance

This section categorizes missing features based on their critical need for modern open-source software stacks (e.g., browsers, databases, language runtimes).

### Critical Importance
Features required by widely used modern software. Absence often requires non-trivial patches or disables core functionality.

1.  **Open File Description Locks (OFD Locks)**
    *   **Why:** Essential for **SQLite** (Write-Ahead Logging mode), **systemd**-style daemons, and modern multi-threaded tools to prevent accidental lock release when closing file descriptors.
    *   **Impact:** Database reliability and high-concurrency file access.

2.  **Robust Mutexes (`PTHREAD_MUTEX_ROBUST`)**
    *   **Why:** Required for reliable shared-memory IPC in databases like **LMDB** and **PostgreSQL**. Prevents deadlocks if a process crashes while holding a lock in shared memory.
    *   **Impact:** Data integrity in crash scenarios for IPC-heavy applications.

3.  **Memory Locking (`mlock`, `mlockall`)**
    *   **Why:** Critical for **Cryptographic tools** (GnuPG, SSH agents) to prevent swapping secrets to disk. Also essential for **Real-time Audio** (JACK, Ardour) to prevent page faults during audio processing.
    *   **Impact:** Security (leaking keys) and Multimedia performance (audio dropouts).

4.  **Termios Completeness**
    *   **Why:** Absolute requirement for interactive CLI tools, shells (**Bash**, **Zsh**), editors (**Vim**, **Emacs**), and terminal multiplexers (**tmux**).
    *   **Impact:** User experience in the terminal; broken hotkeys or display artifacts.

### Useful / Legacy
Features used by specific classes of software but less critical for general modern application support.

1.  **Asynchronous I/O (AIO)**
    *   **Why:** Required for some enterprise databases (DB2, Oracle) and older high-performance servers (Lighttpd). Modern Linux software often prefers `epoll` or `io_uring`, but AIO remains the portable standard.
    *   **Impact:** Compatibility with specific high-throughput I/O applications.

2.  **User Contexts (`ucontext`)**
    *   **Why:** Used by legacy coroutine libraries and some language runtimes (older Go versions, green thread implementations). Often replaced by architecture-specific assembly in modern compilers.
    *   **Impact:** Porting of specific language runtimes or emulation cores.

3.  **XSI Shared Memory / Message Queues**
    *   **Why:** Used by older X11 extensions, legacy databases, and some industrial/embedded software.
    *   **Impact:** Compatibility with legacy UNIX software.

### Obsolescent / Niche
Features rarely used in modern software development.

1.  **STREAMS:** Effectively dead in modern open source.
2.  **POSIX Trace:** Superseded by DTrace, eBPF, or ptrace.
3.  **Typed Memory:** Restricted to specialized real-time/embedded domains.

---

## Missing Features

### 1. Open File Description Locks (OFD)
*   **Missing Macro:** `F_OFD_SETLK` (and `F_OFD_SETLKW`, `F_OFD_GETLK`).
*   **Description:** Advisory byte-range locks associated with the open file description, rather than the process. Useful for multi-threaded applications to avoid accidental unlocking by other threads.
*   **Difficulty:** **Medium**. Requires kernel VFS changes to track locks per file description rather than per process-file pair.
*   **Importance:** **Medium (Critical for Modern DBs)**. Increasingly used by modern database libraries (e.g., SQLite) and multi-threaded tools.
*   **Affected Areas:**
    *   **Headers:** `fcntl.h` (add macros).
    *   **Kernel:** VFS subsystem (lock tracking), `fcntl` syscall handler.
    *   **Libroot:** Minimal impact (syscall wrapper).

### 2. Asynchronous I/O (AIO)
*   **Missing Header:** `<aio.h>`
*   **Description:** Functions for asynchronous read/write operations (`aio_read`, `aio_write`, `lio_listio`, etc.).
*   **Difficulty:** **High**. Requires significant kernel support to be truly asynchronous and efficient.
*   **Importance:** **Medium**. Critical for high-performance servers and databases (e.g., PostgreSQL, Nginx).
*   **Affected Areas:**
    *   **Headers:** `aio.h` (create new header).
    *   **Kernel:** Async I/O subsystem, VFS/driver support.
    *   **Libroot:** Implementation of library functions (`aio_*`).

### 3. POSIX Message Queues
*   **Missing Header:** `<mqueue.h>`
*   **Description:** Named message queues (`mq_open`, `mq_send`, `mq_receive`).
*   **Status:** XSI (System V) message queues are present, but POSIX ones are missing. `os-test` gives a score of 8% for `MSG`.
*   **Difficulty:** **Medium**. Requires kernel IPC implementation and user-space library support.
*   **Importance:** **Medium**. Used in real-time applications and modern IPC designs.
*   **Affected Areas:**
    *   **Headers:** `mqueue.h`.
    *   **Kernel:** IPC subsystem (new message queue primitives).
    *   **Libroot:** User-space wrapper functions.

### 4. User Context Switching
*   **Missing/Incomplete:** `<ucontext.h>` functions (`getcontext`, `setcontext`, `makecontext`, `swapcontext`).
*   **Description:** Mechanism for user-level context switching.
*   **Difficulty:** **High**. Architecture-dependent assembly implementation required for each supported platform (x86, x86_64, ARM, etc.).
*   **Importance:** **High**. Essential for language runtimes (Go), coroutine libraries, and some emulators.
*   **Affected Areas:**
    *   **Headers:** `ucontext.h`.
    *   **Libroot:** Architecture-specific assembly (`arch/` directories).
    *   **Kernel:** Potential integration for signal handling/context restoration.

### 5. Robust Mutexes
*   **Missing Feature:** `PTHREAD_MUTEX_ROBUST`.
*   **Description:** Allows recovering a mutex if the owner dies while holding it.
*   **Status:** Macros `_POSIX_THREAD_ROBUST_PRIO_INHERIT` and `_POSIX_THREAD_ROBUST_PRIO_PROTECT` are defined as `-1` (unsupported) in `unistd.h`.
*   **Difficulty:** **Medium**. Requires kernel support to detect thread death and mark the mutex state.
*   **Importance:** **Medium (High for IPC)**. Important for reliable shared-memory applications and databases.
*   **Affected Areas:**
    *   **Headers:** `pthread.h`.
    *   **Libroot:** `pthread_mutex_*` implementation.
    *   **Kernel:** Thread death notification to mutex objects (robust list support).

### 6. Process Tracing
*   **Missing Header:** `<trace.h>`
*   **Description:** POSIX Trace standard.
*   **Difficulty:** **High**.
*   **Importance:** **Low**. Most systems use platform-specific tracing (ptrace, DTrace) or debuggers.
*   **Affected Areas:**
    *   **Headers:** `trace.h`.
    *   **Kernel:** Tracing infrastructure.
    *   **Libroot:** Trace control functions.

### 7. STREAMS
*   **Missing Header:** `<stropts.h>`
*   **Description:** STREAMS interface.
*   **Difficulty:** **High**.
*   **Importance:** **Low**. Obsolescent and rarely used in modern software.
*   **Affected Areas:**
    *   **Headers:** `stropts.h`.
    *   **Kernel:** Network/Driver stack overhaul.
    *   **Libroot:** IOCTLs and stream functions.

### 8. XSI Shared Memory (Headers)
*   **Missing Header:** `<sys/shm.h>`
*   **Description:** System V style shared memory headers. POSIX shared memory (`shm_open`) is implemented.
*   **Status:** `os-test` indicates incomplete support (62%) for SHM.
*   **Difficulty:** **Medium**.
*   **Importance:** **Medium**. Required for legacy applications.
*   **Affected Areas:**
    *   **Headers:** `sys/shm.h`.
    *   **Kernel:** XSI SHM implementation (likely mapping to existing shared memory primitives).
    *   **Libroot:** `shm*` functions.

### 9. Process Memory Locking
*   **Missing Feature:** `mlockall`, `munlockall`.
*   **Status:** `os-test` shows 0% score for `ML` (Process Memory Locking).
*   **Description:** Lock all of a process's address space into RAM.
*   **Difficulty:** **Medium**.
*   **Importance:** **Low/Medium (Critical for Crypto/Audio)**. Real-time applications and security keys.
*   **Affected Areas:**
    *   **Headers:** `sys/mman.h`.
    *   **Kernel:** VM subsystem (wiring pages).
    *   **Libroot:** Syscall wrappers.

### 10. Typed Memory Objects
*   **Missing Feature:** `posix_typed_mem_open` and related functions.
*   **Status:** `os-test` shows 0% score for `TYM`.
*   **Description:** Advanced memory mapping interface.
*   **Difficulty:** **High**.
*   **Importance:** **Low**. Rarely used outside specialized embedded/RTOS contexts.
*   **Affected Areas:**
    *   **Headers:** `sys/mman.h`.
    *   **Kernel:** VM subsystem.
    *   **Libroot:** Wrappers.

### 11. Advanced mmap Flags
*   **Missing Flags:** `MAP_STACK`, `MAP_HUGETLB`, `MAP_POPULATE`, `MAP_NONBLOCK`.
*   **Description:** Specialized mapping behaviors.
*   **Difficulty:** **Medium/High**. Depends on kernel VM capabilities (huge pages, stack guard pages).
*   **Importance:** **Low**. Mostly optimizations; software usually has fallbacks.
*   **Affected Areas:**
    *   **Headers:** `sys/mman.h`.
    *   **Kernel:** VM subsystem (`vm_map_file` handling).

## Detailed Basic Compliance (`os-test/basic`)

Based on results from the `os-test` basic suite ([link](https://sortix.org/os-test/basic/#basic)), Haiku achieves an overall score of **93%** (397/426 tests passing).

### Specific Failure Areas:
*   **Termios (`termios`): 69% (9/13 passed)**
    *   **Impact:** CLI applications may behave incorrectly regarding terminal control.
    *   **Affected Areas:** `libroot` (`termios` implementation), `kernel` (tty driver ioctls).
*   **System Status (`sys_stat`): 85% (12/14 passed)**
    *   **Impact:** File metadata retrieval discrepancies.
    *   **Affected Areas:** `libroot` (`stat` wrappers), `kernel` (VFS stat implementation).
*   **Process Scheduling (`PS` option): 0% (0/4 passed)**
    *   **Impact:** `sched_*` functions are likely missing or stubbed.
    *   **Affected Areas:** `headers` (`sched.h`), `libroot`, `kernel` (scheduler API).
*   **Thread Execution Scheduling (`TPS` option): 44% (4/9 passed)**
    *   **Impact:** Thread priority and policy management via pthreads is incomplete.
    *   **Affected Areas:** `libroot` (`pthread` library), `kernel` (thread priority mapping).
*   **Robust Mutex Priorities (`RPP|TPP` option): 0% (0/4 passed)**
    *   **Impact:** Priority inheritance/protection for mutexes is missing.
    *   **Affected Areas:** `libroot` (mutex init), `kernel` (scheduler priority boost).

## General Compliance Verification (os-test)

Overall `os-test` suite scores indicate the following gaps:
*   **I/O (`io` suite):** **40%**. Significant gaps in input/output system calls.
*   **Limits (`limits` suite):** **35%**. `limits.h` constants may be incorrect or missing.
*   **Namespace (`namespace` suite):** **20%**. Significant namespace pollution in standard headers.
*   **Process (`process` suite):** **65%**. Missing or non-compliant process system calls.
*   **Terminal (`pty` suite):** **62%**. Pseudoterminal support issues.

### Verified Implemented Features:
The following features are implemented in the codebase, though corner cases may exist:
*   `pthread_cancel`: Implemented (supports asynchronous cancellation).
*   `pthread_cond_init`: Implemented (supports `CLOCK_MONOTONIC`).
*   `pthread_cond_timedwait`: Implemented (respects clock attribute).
*   `pthread_detach`: Implemented.
*   `pthread_rwlock_*`: Implemented (Read/Write/Timed/Try variants present).
*   `mmap`: **100% Score (os-test)**. Implemented with support for `MAP_SHARED`, `MAP_PRIVATE`, `MAP_FIXED`, `MAP_ANONYMOUS`.
*   `strftime`: **95% Score (os-test/time)**. Implemented via musl (`src/system/libroot/posix/musl/time/strftime.c`), fully supporting locale (`strftime_l`).

## Improvements & Partial Implementations

### 1. Thread Scheduling Attributes
*   **Issue:** `TPS` (Thread Execution Scheduling) scores **44%** (basic suite) / **64%** (full suite). Functions like `pthread_attr_setinheritsched` are declared but often return errors or are stubs.
*   **Difficulty:** **Medium**. Requires binding pthread attributes to kernel scheduler parameters.
*   **Importance:** **Medium**. Crucial for real-time compliance.
*   **Affected Areas:**
    *   **Libroot:** `pthread_attr_*` functions.
    *   **Kernel:** Scheduler support for `SCHED_FIFO`/`SCHED_RR` per thread.

### 2. Complex Math
*   **Issue:** `<complex.h>` and `<tgmath.h>` support should be verified against C99/C11 compliance.
*   **Difficulty:** **Low**.
*   **Importance:** **Medium**. Scientific computing.
*   **Affected Areas:**
    *   **Headers:** `complex.h`, `tgmath.h`.
    *   **Libroot:** Math library implementation (`libm`).

### 3. Word Expansion
*   **Missing Function:** `wordexp()` from `<wordexp.h>`.
*   **Description:** Shell-like word expansion.
*   **Difficulty:** **Low/Medium**. Can be ported from musl/glibc.
*   **Importance:** **Low**. Used by CLI utilities.
*   **Affected Areas:**
    *   **Headers:** `wordexp.h`.
    *   **Libroot:** Implementation of word expansion logic.
