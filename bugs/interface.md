# Interface Kit Bugs

## 1. "Zombie" Auto-repeat in `SpinnerButton`
In `src/kits/interface/AbstractSpinner.cpp`, the `SpinnerButton` class implements auto-repeat using `BMessageRunner`. However, `MessageReceived` only checks the `fIsMouseDown` flag and does not verify that the mouse button is still actually pressed (e.g., using `GetMouse`). This can lead to "zombie" auto-repeat actions if the `MouseUp` event is missed or if the state becomes desynchronized.
