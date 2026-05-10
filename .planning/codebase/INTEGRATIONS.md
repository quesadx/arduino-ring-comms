# External Integrations

**Analysis Date:** 2026-05-10

## APIs & External Services

**None.** This is a standalone embedded system with no network stack, no external API calls, and no internet connectivity. All communication happens over physical hardware interfaces.

## Data Storage

**Databases:**
- None — no persistent storage, no EEPROM usage, no SD card

**File Storage:**
- None

**Caching:**
- None

## Authentication & Identity

**Auth Provider:**
- None — no authentication mechanism
- Device identity is a hardcoded 4-bit ID (`MY_ID`): `0x1` in `tx/tx.ino`, `0x2` in `rx/rx.ino`
- XOR key (`0x42`) provides basic payload obfuscation, not authentication

## Monitoring & Observability

**Error Tracking:**
- None — no external error tracking service

**Logs:**
- Serial output at 9600 baud provides human-readable debug/log messages:
  - `tx/tx.ino:71-72`: Status messages (`"TX Listo. Escribe mensaje:"`, `">> Enviado."`)
  - `rx/rx.ino:61`: Status message (`"RX Listo. Esperando laser..."`)
  - `rx/rx.ino:90-99`: Decoded payload output and frame validation error details (checksum mismatch, footer mismatch, raw payload hex dump)

## CI/CD & Deployment

**Hosting:**
- Not applicable — firmware is uploaded directly to Arduino board via USB

**CI Pipeline:**
- None — no CI/CD configuration present

## Environment Configuration

**Required env vars:**
- None — no environment variables used

**Secrets location:**
- No secrets or credentials in the codebase
- The XOR key (`0x42`) is hardcoded in both sketches — not treated as a secret

## Webhooks & Callbacks

**Incoming:**
- None

**Outgoing:**
- None

## Hardware Interfaces

### Digital I/O (TX: `tx/tx.ino`)
| Pin | Mode | Purpose | Constant |
|-----|------|---------|----------|
| 8   | OUTPUT | Laser module control | `LASER_PIN` |

### Digital I/O (RX: `rx/rx.ino`)
| Pin | Mode | Purpose | Constant |
|-----|------|---------|----------|
| 7   | INPUT | Photodetector / light sensor input | `RX_PIN` |

### Serial
| Baud | Usage |
|------|-------|
| 9600 | User interface: TX accepts typed messages, RX prints received/decoded messages |

### Communication Protocol
- **Physical layer:** Optical (laser on/off keying) via digital GPIO
- **Encoding:** Manchester (self-clocking, DC-balanced)
- **Frame format:** `[START_HIGH] [START_LOW] [0xAA preamble] [IDs byte] [Seq+Len byte] [Payload...] [Checksum] [0x55 footer] [END_GUARD]`
- **Timing:** Half-bit period = 200ms (`BIT_HALF_US = 200000µs`), resulting in ~2.5 bits/second data rate
- **Encryption:** XOR with key `0x42` on payload only

---

*Integration audit: 2026-05-10*
