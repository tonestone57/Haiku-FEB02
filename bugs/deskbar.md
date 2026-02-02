# Deskbar Bugs

## 1. Unhandled `std::bad_alloc` in `BarViewMessageFilter`
In `src/apps/deskbar/BarView.cpp`, the `Filter` method (line 155) allocates a `BMessage` using `new BMessage()`. In Haiku, `new` can throw `std::bad_alloc` if memory is low. This allocation is not wrapped in a try-catch block, nor does it use `(std::nothrow)`, which could lead to an application crash in low-memory situations.

## 2. Inconsistent use of `(std::nothrow)`
In `src/apps/deskbar/BarApp.cpp` and other files in Deskbar, many `new` calls do not use `(std::nothrow)`, while others (like in `Subscribe`) do. This inconsistency makes the code harder to audit for memory safety and robustness in low-memory conditions.
