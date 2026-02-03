# Audio Driver Bugs

## 1. Variable Length Array (VLA) in `hda_multi_audio.cpp`
In `src/add-ons/kernel/drivers/audio/hda/hda_multi_audio.cpp`, the `get_buffers` function (line 741 and 761) uses VLAs:
```cpp
struct buffer_desc descs[data->return_playback_channels];
```
and
```cpp
struct buffer_desc descs[data->return_record_channels];
```
If the number of channels is large, this could lead to a kernel stack overflow. It's better to use a fixed-size buffer or allocate on the heap.

## 2. Potential Response Loss in `hda_controller.cpp`
In `src/add-ons/kernel/drivers/audio/hda/hda_controller.cpp`, the `hda_interrupt_handler` (line 330) skips responses if `codec->response_count` reaches `MAX_CODEC_RESPONSES` (16). While it prints a warning, this can lead to the driver getting out of sync with the hardware, as the corresponding semaphore is still released, potentially causing `hda_send_verbs` to proceed with incomplete or incorrect data.

## 3. Potential Buffer Overflows in `hda_codec.cpp`
Several functions in `src/add-ons/kernel/drivers/audio/hda/hda_codec.cpp` use `sprintf` to build strings in fixed-size buffers:
- `dump_widget_audio_capabilities` (line 218): uses `buffer[256]` to store concatenated flag names.
- `dump_widget_inputs` (line 240): uses `buffer[256]` for input lists.
Using `snprintf` would be safer to prevent potential overflows if many items are present.
