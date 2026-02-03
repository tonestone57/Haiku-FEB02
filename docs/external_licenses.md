# External Software Licenses in Haiku OS

This document lists the various software licenses used by external code integrated into the Haiku OS source tree.

## MIT License
*   **Musl Libc**: `src/system/libroot/posix/musl`
*   **Zydis Disassembler**: `src/libs/zydis`
*   **X86 Emulator**: `src/libs/x86emu`
*   **GLUT** (Haiku implementation/derivative): `src/libs/glut`
*   **GNU extensions** (Haiku implementation): `src/libs/gnu`
*   **Network Compatibility Layers**: `src/libs/compat/openbsd_network` (often MIT/BSD dual or similar)

## BSD Licenses (3-Clause / 2-Clause)
*   **Libsolv** (BSD 3-Clause): `src/libs/libsolv`
*   **UUID Library** (BSD 3-Clause): `src/libs/uuid`
*   **Libtelnet** (BSD 3-Clause): `src/libs/libtelnet`
*   **GLTeapot** (Be Sample Code License - BSD-like): `src/apps/glteapot`
*   **Libfdt** (BSD 2-Clause / GPL Dual): `src/libs/libfdt`
*   **FreeBSD Compatibility Layer** (BSD): `src/libs/compat/freebsd_network`, `src/libs/compat/freebsd_wlan`
*   **OpenBSD Compatibility Layer** (BSD): `src/libs/compat/openbsd_wlan`

## GNU General Public License (GPL)
*   **Libntfs** (GPL v2 or later): `src/add-ons/kernel/file_systems/ntfs/libntfs`
*   **Libfdt** (GPL v2 or later / BSD Dual): `src/libs/libfdt`
*   **PCMCIA Tools** (GPL): `src/bin/pcmcia-cs` (Tools part)

## GNU Lesser General Public License (LGPL)
*   **GNU C Library (glibc)** (LGPL v2.1 or later): `src/system/libroot/posix/glibc`

## Mozilla Public License (MPL)
*   **PCMCIA Card Services** (MPL v1.1): `src/bin/pcmcia-cs` (Core)

## ISC License
*   **Libiconv** (ISC): `src/libs/iconv`
*   **OpenBSD Malloc** (ISC): `src/system/libroot/posix/malloc/openbsd`

## Anti-Grain Geometry License
*   **Anti-Grain Geometry (AGG)**: `src/libs/agg`
    *   *Permissive license requiring copyright notice.*

## MAPM License
*   **Mike's Arbitrary Precision Math Library (MAPM)**: `src/libs/mapm`
    *   *Freeware license allowing use, copy, distribution, and modification.*

## Info-ZIP License
*   **UnZip**: `src/bin/unzip`
    *   *Permissive license with specific attribution requirements.*

## Zlib License
*   **Zlib**: `src/system/kernel/lib/zlib` (build integration)
    *   *Standard Zlib license.*

> **Note:** This list is based on an analysis of the source tree and may not be exhaustive. External dependencies fetched during the build process are not included.
