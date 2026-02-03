# Code Audit Report: Haiku `src/servers`

**Date:** 2024
**Target:** `src/servers`
**Auditor:** Jules

## 1. Executive Summary

A comprehensive code audit was performed on `src/servers/app`, `src/servers/net`, `src/servers/media`, and `src/servers/keystore`. The audit identified **3 Critical Vulnerabilities** involving stack buffer overflows and missing cryptographic implementations, as well as several logic errors and race conditions.

## 2. Critical Vulnerabilities

### 2.1. Stack Buffer Overflow in `DHCPClient`
**File:** `src/servers/net/DHCPClient.cpp`
**Function:** `dhcp_message::PutOption` (Line 365)
**Description:**
The `PutOption` function blindly copies data into the `options` array without checking if there is sufficient space remaining in the buffer.
```cpp
uint8*
dhcp_message::PutOption(uint8* options, message_option option,
    const uint8* data, uint32 size)
{
    // TODO: check enough space is available
    options[0] = option;
    options[1] = size;
    memcpy(&options[2], data, size);
    return options + 2 + size;
}
```
**Impact:** Remote Code Execution (RCE) or Denial of Service (DoS). A malicious DHCP server (or compromised network) could send a large option payload, causing a stack overflow in the client process.

### 2.2. Stack Buffer Overflow in `DWindowHWInterface`
**File:** `src/servers/app/drawing/interface/virtual/DWindowHWInterface.cpp`
**Function:** `_OpenAccelerant` (Lines 466-467)
**Description:**
The function concatenates user-controlled (or environment-controlled) strings into a fixed-size `path` buffer (`PATH_MAX`, usually 1024) without bounds checking.
```cpp
strcat(path, "/accelerants/");
strcat(path, signature); // signature is a 1024-byte buffer
```
**Impact:** Local Privilege Escalation or Crash. If `signature` (obtained via ioctl) is large, it will overflow `path`.

### 2.3. Missing Encryption in `KeyStoreServer`
**File:** `src/servers/keystore/Keyring.cpp`
**Function:** `_EncryptToFlatBuffer` (Line 434)
**Description:**
The code responsible for encrypting the keyring data is unimplemented.
```cpp
if (fHasUnlockKey) {
    // TODO: Actually encrypt the flat buffer...
}
```
**Impact:** Information Disclosure. Passwords and keys stored in the system keystore are saved to disk in plain text (serialized BMessage), even if a master password is set.

## 3. High/Medium Risks

### 3.1. Buffer Overflow in `DWindowHWInterface::_OpenGraphicsDevice`
**File:** `src/servers/app/drawing/interface/virtual/DWindowHWInterface.cpp`
**Line:** 424
**Description:**
Unsafe use of `sprintf` with `entry->d_name`.
```cpp
sprintf(path, "/dev/graphics/%s", entry->d_name);
```
**Recommendation:** Replace with `snprintf`.

### 3.2. Race Condition in `StackAndTile`
**File:** `src/servers/app/stackandtile/StackAndTile.cpp`
**Line:** 532
**Description:**
A known race condition exists where `WindowAdded` might be called concurrently, leading to duplicate SATWindow creation.
**Status:** Fixed. Implemented lazy initialization in `GetSATWindow` and existence check in `WindowAdded`.

### 3.3. Memory Management in `ClientMemoryAllocator`
**File:** `src/servers/app/ClientMemoryAllocator.cpp`
**Description:**
Several TODOs indicate inefficient memory management and potential leaks (e.g., "The HashTable grows without bounds").

### 3.4. Unsafe String Handling in `Registrar`
**File:** `src/servers/registrar/TRoster.cpp`, `src/servers/registrar/ShutdownProcess.cpp`
**Description:**
Use of `strcpy` for `app_info` strings. While source strings are often system-managed, `strlcpy` offers better defense-in-depth against potential overflow if struct definitions change or data is corrupted.
**Status:** Fixed. Replaced `strcpy` with `strlcpy`.

## 4. Recommendations

1.  **Immediate Fixes:**
    *   Implement bounds checking in `dhcp_message::PutOption`.
    *   Replace `sprintf`/`strcat` with `snprintf`/`strlcat` in `DWindowHWInterface.cpp`.
    *   Implement encryption in `Keyring.cpp` (or at least warn the user that it is disabled).

2.  **Long-term:**
    *   Conduct a fuzzy testing campaign against `DHCPClient`.
    *   Refactor `ServerWindow` message handling to ensure `malloc` failures are gracefully handled for large string inputs.
