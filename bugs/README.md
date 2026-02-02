# Haiku Coding Errors and Bugs

This directory contains Markdown files describing various coding errors and bugs found in the Haiku source code.

## Files
- [icon-o-matic.md](icon-o-matic.md): Bugs in the Icon-O-Matic application.
- [installer.md](installer.md): Bugs in the Installer application (CopyEngine, UnzipEngine, WorkerThread).
- [interface.md](interface.md): Bugs in the Interface Kit (AbstractSpinner).
- [usb.md](usb.md): Bugs in the USB driver (UHCI).
- [more_bugs.md](more_bugs.md): Bugs in Terminal and Debugger.
- [deskbar.md](deskbar.md): Bugs in the Deskbar application.
- [tracker.md](tracker.md): Bugs in the Tracker application and kit.
- [kernel.md](kernel.md): Bugs in the Haiku Kernel.

## Summary of Bugs Found
- **Icon-O-Matic**: Missing allocation checks, potential NULL pointer dereferences in command creation, and problematic use of Variable Length Arrays (VLAs).
- **Installer**: Use-after-free in `CopyEngine`, buffer overflows in `WorkerThread` and `PackageViews`, and incorrect parsing of filenames with spaces in `UnzipEngine`.
- **USB Driver**: Race condition in UHCI transfer submission.
- **Interface Kit**: "Zombie" auto-repeat issue in `SpinnerButton`.
- **Terminal & Debugger**: Missing allocation checks, potential memory leaks, and blocking handshake protocols.
- **Deskbar**: Unhandled exceptions and inconsistent memory management.
- **Tracker**: Memory leaks and unchecked string operations.
- **Kernel**: Race conditions during lock destruction, team ID wrap-around issues, missing deadlock detection in VFS, unreliable resource iteration, and excessive stack usage.
