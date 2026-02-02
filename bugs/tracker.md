# Tracker Bugs

## 1. Memory Leak in `BDeskWindow::ApplyShortcutPreferences`
In `src/kits/tracker/DeskWindow.cpp`, the `ApplyShortcutPreferences` method (line 347) allocates `fShortcutsSettings` using `new char[...]` but does not check if `fShortcutsSettings` already points to an existing allocation. Since `ApplyShortcutPreferences` can be called multiple times (e.g., when preferences are updated), this leads to a memory leak of the previous path string.

## 2. Unchecked `strcpy` in `BPoseView::CreateClippingFile`
In `src/kits/tracker/PoseView.cpp`, the `CreateClippingFile` method (line 4400) uses `strcpy(resultingName, fallbackName);`. While `resultingName` is typically allocated with `B_FILE_NAME_LENGTH` (1024 bytes), if a very long `fallbackName` is passed, it could theoretically cause a buffer overflow. Using `strlcpy` is safer.

## 3. Potential NULL pointer dereference in `StringToScalar`
In `src/kits/tracker/Utilities.cpp`, the `StringToScalar` method (line 1279) uses `new char [strlen(text) + 1]`. If this allocation fails (and it would return `NULL` if `(std::nothrow)` were used, or throw `std::bad_alloc` otherwise), the subsequent `strcpy(buffer, text)` will dereference a NULL pointer or the exception will go uncaught.
