# App Server Bugs

## 1. Buffer Overflow in `DWindowHWInterface::_OpenAccelerant`
In `src/servers/app/drawing/interface/virtual/DWindowHWInterface.cpp`, the path construction for accelerants uses unsafe `strcat` calls on a fixed-size buffer:

```cpp
char path[PATH_MAX];
if (find_directory(dirs[i], -1, false, path, PATH_MAX) != B_OK)
    continue;

strcat(path, "/accelerants/");
strcat(path, signature);
```

While `PATH_MAX` is typically 1024, the `signature` buffer (retrieved via `ioctl` with `B_GET_ACCELERANT_SIGNATURE`) is also 1024 bytes. If `find_directory` returns a path of significant length, appending `/accelerants/` and then a potentially long `signature` will overflow the `path` buffer. This can lead to memory corruption or crashes in the `app_server`.

## 2. Unsafe `sprintf` in `DWindowHWInterface::DWindowHWInterface`
In the constructor of `DWindowHWInterface`, `sprintf` is used to build a path for cloning:

```cpp
sprintf((char*)cloneInfoData, "graphics/%s", fCardNameInDevFS.String());
```

If `fCardNameInDevFS` is long, it could overflow `cloneInfoData`. This should use `snprintf` to ensure safety.

## 3. Unsafe `sprintf` in Virtual HW Interfaces
Several virtual hardware interface implementations use `sprintf` to fill fixed-size info buffers:

```cpp
// src/servers/app/drawing/interface/virtual/ViewHWInterface.cpp
sprintf(info->name, "Haiku, Inc. ViewHWInterface");
sprintf(info->chipset, "Haiku, Inc. Chipset");
sprintf(info->serial_no, "3.14159265358979323846");
```

While these specific strings are currently safe, using `strlcpy` or `snprintf` is a better practice to prevent future overflows if the strings are changed or if the buffers in the `info` structure are smaller than expected.
