# Mail Bugs

## 1. Fixed-size Buffer for MD5 Digest in POP3
In `src/add-ons/mail_daemon/inbound_protocols/pop3/POP3.cpp`, the `MD5Digest` method (line 539) assumes `asciiDigest` is large enough to hold 32 characters plus a null terminator. While its current caller provides a 33-byte buffer, this design is fragile and lacks bounds checking, making it susceptible to future overflows if used elsewhere.

## 2. Unpurged Manifest in POP3
The `CheckForDeletedMessages` function in `src/add-ons/mail_daemon/inbound_protocols/pop3/POP3.cpp` contains a TODO (line 462) stating that the purged manifest should be written to disk, otherwise it will grow forever. This indicates a potential resource leak where the manifest file continues to increase in size over time as messages are processed.
