# Icon-O-Matic Bugs

## 1. Missing Allocation Checks in `ShapeListView::InstantiateSelection`
In `src/apps/icon-o-matic/gui/ShapeListView.cpp`, the `InstantiateSelection` method creates several command objects and an array of commands using `new(std::nothrow)`, but it does not check if the allocations succeeded. If any of these allocations fail and return `NULL`, it will likely lead to a crash when `fCommandStack->Perform(command)` is called or when the `CompoundCommand` attempts to execute the `NULL` commands.

## 2. Potential Crash in `ShapeListView::CopyItems`
In `src/apps/icon-o-matic/gui/ShapeListView.cpp`, the `CopyItems` method clones shapes using `item->shape->Clone()`. If `Clone()` fails (returns `NULL`), a `NULL` pointer is added to the `shapes` array, which is then passed to `AddShapesCommand`. The constructor of `AddShapesCommand` dereferences the shape pointers to acquire references, which will cause a crash if any pointer is `NULL`.

## 3. Use of Variable Length Arrays (VLAs)
Multiple files in Icon-O-Matic use VLAs, which can cause stack overflow if the number of items is large and may not be compatible with all compilers. Recommended practice in Haiku is to use `new(std::nothrow)` and `delete[]`.
Affected locations:
- `src/apps/icon-o-matic/gui/ShapeListView.cpp`: `MSG_RESET_TRANSFORMATION` and `CopyItems`.
- `src/apps/icon-o-matic/gui/StyleListView.cpp`: `MSG_RESET_TRANSFORMATION`, `CopyItems`, and `RemoveItemList`.

## 4. Missing NULL check for `ResetTransformationCommand`
In `src/apps/icon-o-matic/gui/StyleListView.cpp`, when handling `MSG_RESET_TRANSFORMATION`, the code does not check if `new ResetTransformationCommand` succeeded before passing it to `fCommandStack->Perform()`. Although `Perform()` checks for `NULL`, it's inconsistent with other parts of the code.
