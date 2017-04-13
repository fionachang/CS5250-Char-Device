# CS5250-Char-Device

Linux Character Device

Supported functions:
- `read`
- `write`
- `lseek`
- `ioctl`
  - `setdevmsg`: Set device message by copying the content of the user-supplied character pointer
  - `getdevmsg`: Get device message by copying it to the user-supplied character pointer
  - `getsetdevmsg`: Set device message and get original device message
