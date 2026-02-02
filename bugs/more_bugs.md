# More Haiku Bugs

## 1. Missing Allocation Checks in Terminal
In `src/apps/terminal/TermView.cpp`, `_GetArgumentsFromMessage` (line 1002) allocates an array for arguments using `new` but does not check if it succeeded. This could lead to a crash if the allocation fails.

## 2. Potential Memory Leak in SourceView
In `src/apps/debugger/user_interface/gui/team_window/SourceView.cpp`, the `MouseMoved` method (line 1033) creates a new `BMessageRunner` when the mouse exits the view. It does not check if `fScrollRunner` is already non-NULL, which could lead to a memory leak if multiple `B_EXITED_VIEW` events are received without corresponding `B_ENTERED_VIEW` events.

## 3. Handshake Blocking in Shell
In `src/apps/terminal/Shell.cpp`, the parent process waits for a handshake from the child using `receive_handshake_message` (which calls `receive_data`). If the child process fails to start or crashes before sending the expected handshake, the parent (the Terminal application) may hang indefinitely.
