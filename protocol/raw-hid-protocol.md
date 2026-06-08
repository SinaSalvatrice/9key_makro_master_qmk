# Raw HID Protocol (Custom, Keyboard-Specific)

This document defines a lightweight custom Raw HID protocol for the 9-Key Macro Master.

This is not full VIA protocol compatibility.

## Transport

- Channel: USB HID Raw
- Packet size: 32 bytes
- Direction: host <-> keyboard
- Scope: keyboard-specific configuration and key mapping operations

## Packet Layout

Each packet is 32 bytes.

- Byte 0: protocol version (current: 1)
- Byte 1: command
- Byte 2: request id (echoed in response)
- Byte 3: status (responses only)
- Bytes 4-31: payload

Status values:

- 0x00: OK
- 0x01: UNKNOWN_COMMAND
- 0x02: INVALID_ARGUMENT
- 0x03: UNSUPPORTED
- 0x04: INTERNAL_ERROR

## Commands (Initial)

- 0x01: PING
- 0x02: GET_INFO
- 0x10: GET_KEY
- 0x11: SET_KEY
- 0x20: SAVE_EEPROM

### 0x01 PING

Request payload:

- Bytes 4-7: optional host nonce

Response payload:

- Bytes 4-7: echoed nonce
- Bytes 8-11: ASCII "PONG"

### 0x02 GET_INFO

Request payload:

- none

Response payload:

- Byte 4: rows (3)
- Byte 5: cols (3)
- Byte 6: layers (4)
- Byte 7: encoders (1)
- Byte 8: packet size (32)
- Bytes 9-24: keyboard id as ASCII, zero-padded

### 0x10 GET_KEY

Request payload:

- Byte 4: layer
- Byte 5: row
- Byte 6: col

Response payload:

- Bytes 4-5: keycode (uint16, little-endian)

### 0x11 SET_KEY

Request payload:

- Byte 4: layer
- Byte 5: row
- Byte 6: col
- Bytes 7-8: keycode (uint16, little-endian)

Response payload:

- Byte 4: applied flag (1 = yes, 0 = no)

### 0x20 SAVE_EEPROM

Request payload:

- none

Response payload:

- Byte 4: save result (1 = saved)

## Notes

- Host should send one request at a time and match responses by request id.
- Unknown commands must return status UNKNOWN_COMMAND.
- Protocol evolution should increment version and preserve backward compatibility where possible.
- Keep command scope narrow and keyboard-specific.
