# Haiku Coding Errors and Bugs

This directory contains Markdown files describing various coding errors and bugs found in the Haiku source code.

## Files
- [icon-o-matic.md](icon-o-matic.md): Bugs in the Icon-O-Matic application.
- [installer.md](installer.md): Bugs in the Installer application (CopyEngine, UnzipEngine, WorkerThread).
- [interface.md](interface.md): Bugs in the Interface Kit (AbstractSpinner).
- [usb.md](usb.md): Bugs in the USB driver (UHCI).
- [more_bugs.md](more_bugs.md): Bugs in Terminal and Debugger.

## Summary of Bugs Found
- **Icon-O-Matic**: Missing allocation checks, potential NULL pointer dereferences in command creation, and problematic use of Variable Length Arrays (VLAs).
- **Installer**: Use-after-free in `CopyEngine`, buffer overflows in `WorkerThread`, and incorrect parsing of filenames with spaces in `UnzipEngine`.
- **USB Driver**: Race condition in UHCI transfer submission.
- **Interface Kit**: "Zombie" auto-repeat issue in `SpinnerButton`.
- **Terminal & Debugger**: Missing allocation checks, potential memory leaks, and blocking handshake protocols.
