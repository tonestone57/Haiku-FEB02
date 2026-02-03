# Network Stack Code Audit

**Date:** October 26, 2023
**Scope:** `src/add-ons/kernel/network` and subdirectories.

## Executive Summary

A comprehensive audit of the Haiku network stack revealed several issues ranging from potential denial-of-service vectors (deadlocks) to resource management bugs and legacy unsafe code patterns. The core TCP/IP stack is generally well-structured but contains subtle logic errors in edge cases. The `ppp` and `devices` directories contain more legacy code issues (unsafe string handling).

## Critical Findings

### 1. TCP `MSG_WAITALL` Deadlock
**Location:** `src/add-ons/kernel/network/protocols/tcp/TCPEndpoint.cpp:580` (`ReadData`)

**Description:**
When `recv()` is called with `MSG_WAITALL`, the `ReadData` function waits until `fReceiveQueue.Available()` is at least `numBytes`. However, the receive queue size is limited by the socket's receive buffer size (`socket->receive.buffer_size`). If the user requests more data than the maximum buffer size (e.g., requesting 1MB when buffer is 64KB), the condition can never be met. The sender will fill the window (64KB), stop sending, and the receiver will wait indefinitely (or until timeout) because it refuses to return partial data, leading to a deadlock.

**Recommendation:**
Cap `dataNeeded` at `fReceiveQueue.Available()` logic or `socket->receive.buffer_size`. The standard behavior for `MSG_WAITALL` allows returning less data if "the next data to be received is of a different type" or arguably if buffers are full, but strictly it should probably be handled by copying data out in chunks or failing the request if it exceeds the maximum possible buffer.
*Suggested Fix:*
```cpp
if (flags & MSG_WAITALL)
    dataNeeded = min_c(numBytes, socket->receive.buffer_size);
```

### 2. Socket Send Partial Failure
**Location:** `src/add-ons/kernel/network/stack/net_socket.cpp:1250` (`socket_send`)

**Description:**
In `socket_send`, the function loops to create and fill buffers. If a partial amount of data has been processed (and potentially sent/buffered by the protocol via `send_data`), and a subsequent `gNetBufferModule.create()` fails (returning `ENOBUFS`), the function returns `ENOBUFS` immediately. This discards the information that some bytes *were* successfully handled. For stream sockets (TCP), this is incorrect; it should return the number of bytes sent (`bytesSent`) if > 0, so the application knows to continue later.

**Recommendation:**
Check `bytesSent` before returning error.
*Suggested Fix:*
```cpp
if (buffer == NULL) {
    if (bytesSent > 0) return bytesSent;
    return ENOBUFS;
}
```

## Medium Severity Findings

### 3. Potential Integer Overflow in Buffer Cloning
**Location:** `src/add-ons/kernel/network/stack/net_buffer.cpp:1322` (`append_cloned_data`)

**Description:**
The check `if (source->size < offset + bytes || source->size < offset)` is vulnerable to integer overflow if `offset + bytes` wraps around `size_t` (or `uint32` depending on architecture). If it wraps to a small value, the check passes, leading to invalid memory access.

**Recommendation:**
Use subtraction to verify bounds safely.
*Suggested Fix:*
```cpp
if (offset > source->size || bytes > source->size - offset)
    return B_BAD_VALUE;
```

### 4. Unsafe String Handling in PPP and Devices
**Location:** Multiple files in `src/add-ons/kernel/network/ppp` and `src/add-ons/kernel/network/devices`.

**Description:**
Widespread use of `strcpy` and `sprintf` into fixed-size buffers was detected. While many usages appear to be copying fixed identifiers, this pattern is fragile and prone to overflows if identifiers change size.
*Examples:*
- `src/add-ons/kernel/network/ppp/ppp/ppp.cpp:89`: `strcpy(device->name, name);`
- `src/add-ons/kernel/network/devices/ethernet/ethernet.cpp:152`: `strcpy(device->name, name);`

**Recommendation:**
Replace all instances with `strlcpy` and `snprintf`.

### 5. TCP RTT Calculation Risk
**Location:** `src/add-ons/kernel/network/protocols/tcp/TCPEndpoint.cpp:1823` (`_UpdateRoundTripTime`)

**Description:**
The calculation `(fSmoothedRoundTripTime + max_c(100, fRoundTripVariation * 4)) * kTimestampFactor` involves multiplication by 1000 (`kTimestampFactor`). While `fSmoothedRoundTripTime` is `int32`, extremely high RTT values (e.g., during network stalls) could cause integer overflow before casting to `bigtime_t`.

**Recommendation:**
Cast to `bigtime_t` (int64) before multiplication.

### 6. Unchecked Memory Allocation
**Location:** `src/add-ons/kernel/network/ppp/pppoe/DiscoveryPacket.cpp:69`

**Description:**
`pppoe_tag *add = (pppoe_tag*) malloc(length + 4);` is not checked for `NULL` before use. This is a potential kernel panic vector if OOM occurs.

**Recommendation:**
Add check for `NULL` return value.

## Low Severity / Maintenance

### 7. Ephemeral Port Prediction
**Location:** `src/add-ons/kernel/network/protocols/udp/udp.cpp`

**Description:**
UDP ephemeral port selection uses a linear scan starting from a `rand()` offset. While typical for non-security-critical OSs, `rand()` is predictable.

### 8. Tech Debt
**Description:**
Numerous `TODO` and `FIXME` comments exist, particularly in `arp` and `ppp`, indicating incomplete features or temporary hacks.

## Conclusion

The audit identified two critical logic bugs in the core stack that should be addressed immediately to prevent deadlocks and incorrect data reporting. The remaining issues are primarily robustness fixes.
