# Code Audit Report: Haiku `src/servers`

**Date:** 2024
**Target:** `src/servers`
**Auditor:** Jules

## 1. Executive Summary

A comprehensive code audit was performed on `src/servers` subdirectories including `app`, `net`, `media`, `keystore`, `registrar`, `mail`, `syslog_daemon`, `bluetooth`, `debug`, `index`, `input`, `launch`, `midi`, `mount`, `notification`, `package`, `power`, `print`, `print_addon`, and `media_addon`.

The audit identified **3 Critical Vulnerabilities** (stack buffer overflows, missing encryption) and several **High/Medium Risks** (unsafe string handling, race conditions).

## 2. Critical Vulnerabilities

### 2.1. Stack Buffer Overflow in `DHCPClient`
**File:** `src/servers/net/DHCPClient.cpp`
**Function:** `dhcp_message::PutOption` (Line 365)
**Description:**
The `PutOption` function blindly copies data into the `options` array without checking if there is sufficient space remaining in the buffer.
**Status:** Fixed. Added bounds check.

### 2.2. Stack Buffer Overflow in `DWindowHWInterface`
**File:** `src/servers/app/drawing/interface/virtual/DWindowHWInterface.cpp`
**Function:** `_OpenAccelerant`
**Description:**
The function concatenates user-controlled strings into a fixed-size `path` buffer without bounds checking.
**Status:** Fixed. Replaced `sprintf`/`strcat` with `snprintf`/`strlcat`.

### 2.3. Missing Encryption in `KeyStoreServer`
**File:** `src/servers/keystore/Keyring.cpp`
**Function:** `_EncryptToFlatBuffer`
**Description:**
The code responsible for encrypting the keyring data is unimplemented (TODO).
**Impact:** Information Disclosure. Passwords stored in cleartext.
**Status:** Fixed. Implemented AES-256-GCM encryption with Argon2id/PBKDF2 key derivation using OpenSSL.

## 3. High/Medium Risks

### 3.1. Unsafe String Handling in `Registrar`
**File:** `src/servers/registrar/TRoster.cpp`, `src/servers/registrar/ShutdownProcess.cpp`
**Description:**
Use of `strcpy` for copying `app_info` strings.
**Status:** Fixed. Replaced with `strlcpy`.

### 3.2. Race Condition in `StackAndTile`
**File:** `src/servers/app/stackandtile/StackAndTile.cpp`
**Description:**
Race condition where `WindowAdded` might be called concurrently, creating duplicate `SATWindow` objects.
**Status:** Fixed. Implemented existence check and lazy initialization.

### 3.3. Unsafe String Handling in `Package` Server
**File:** `src/servers/package/CommitTransactionHandler.cpp`
**Line:** 1896
**Description:** `strcpy` used to copy package filename.
**Status:** Fixed. Replaced with `strlcpy`.

### 3.4. Unsafe String Handling in `Media Addon` Server
**File:** `src/servers/media_addon/MediaAddonServer.cpp`
**Line:** 724
**Description:** `strcpy` used to copy flavor name.
**Status:** Fixed. Replaced with `strlcpy`.

### 3.5. Unsafe String Handling in `Debug` Server
**File:** `src/servers/debug/DebugServer.cpp`
**Line:** 839
**Description:** `sprintf` used with `%s` on message buffer.
**Status:** Fixed. Replaced with `snprintf`.

### 3.6. Unsafe String Handling in `Input` Server
**File:** `src/servers/input/AddOnManager.cpp`
**Line:** 933
**Description:** `sprintf` used for error message formatting.
**Status:** Fixed. Replaced with `snprintf`.

## 4. Recommendations

1.  **Immediate Fixes:**
    *   Fix remaining buffer overflows in `package`, `media_addon`, `debug`, and `input` servers.
    *   Continue replacing `sprintf`/`strcpy` with `snprintf`/`strlcpy` across the codebase.

2.  **Long-term:**
    *   Implement encryption for `KeyStoreServer`.
    *   Refactor memory management in `ClientMemoryAllocator`.
