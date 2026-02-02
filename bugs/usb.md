# USB Driver Bugs

## 1. Race Condition in `UHCI::SubmitTransfer`
In `src/add-ons/kernel/busses/usb/uhci.cpp`, the `SubmitTransfer` method checks the `fHostControllerHalted` flag without holding a lock. The controller state could change immediately after this check. To avoid race conditions, this check should be performed after acquiring the relevant lock (e.g., in `AddPendingTransfer` or `SubmitIsochronous`).
