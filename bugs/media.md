# Media Kit Bugs

## 1. Buffer Overflow in `BTimeSource` Constructor
In `src/kits/media/TimeSource.cpp`, the `BTimeSource` constructor contains a buffer overflow when constructing area names:

```cpp
char name[32];
sprintf(name, "__timesource_buf_%" B_PRId32, id);
...
sprintf(name, "__cloned_timesource_buf_%" B_PRId32, id);
```

The prefix `"__cloned_timesource_buf_"` is 24 characters long. An `int32` ID can be up to 11 characters (including the sign for negative values). Adding these together yields up to 35 characters, plus the null terminator, which exceeds the 32-byte size of the `name` buffer. This can lead to stack corruption.

## 2. Incomplete Error Handling in `BMediaRoster::Connect`
In `src/kits/media/MediaRoster.cpp`, the `Connect` method uses `strcpy` to copy names between potentially untrusted or unvalidated media structures:

```cpp
strcpy(request4.input.name, reply3.name);
```

If `reply3.name` is not properly null-terminated or exceeds the size of `request4.input.name`, it will cause a buffer overflow. Given that `reply3` comes from a `QueryPort` call (interacting with a media node which could be an add-on), this is a potential security or stability risk.

## 3. Potential Name Truncation in `MediaEventLooper`
In `src/kits/media/MediaEventLooper.cpp`, thread names are constructed using `sprintf`:

```cpp
sprintf(threadName, "%.20s control", Name());
```

While the `%.20s` format specifier prevents a buffer overflow on a standard `B_OS_NAME_LENGTH` (32 bytes) buffer, it does so by truncating the node's name. If the node name is important for debugging or identification, this truncation may be undesirable. However, more importantly, other parts of the media kit might not be as careful with thread or port names.
