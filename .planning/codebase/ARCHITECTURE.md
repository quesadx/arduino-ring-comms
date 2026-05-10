# Architecture

**Analysis Date:** 2026-05-10

## System Overview

```text
┌─────────────────────────────────────────────────────────────────┐
│                     Serial Console (User)                        │
│                   9600 baud text interface                       │
└────────────┬───────────────────────────────────┬────────────────┘
             │ (TX only)                         │ (RX only)
             ▼                                   ▼
┌────────────────────────┐          ┌────────────────────────────┐
│     Transmitter (TX)   │  Laser   │       Receiver (RX)        │
│     `tx/tx.ino`        │  ──────▶ │       `rx/rx.ino`         │
│                        │  beam    │                            │
│  • Serial input parser │          │  • Digital signal filter   │
│  • Frame builder       │          │  • Edge detector           │
│  • Manchester encoder  │          │  • Manchester decoder      │
│  • XOR encrypt         │          │  • Frame validator         │
│  • Laser GPIO driver   │          │  • XOR decrypt             │
│  • Hardware timer      │          │  • Serial output           │
└────────────────────────┘          └────────────────────────────┘
         │                                       │
         ▼                                       ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Arduino Hardware Layer                        │
│  `digitalWrite/Read`, `micros`, `delayMicroseconds`, `Serial`   │
└─────────────────────────────────────────────────────────────────┘
         │                                       │
         ▼                                       ▼
┌──────────────────┐                ┌─────────────────────────────┐
│  Laser Module    │                │  Photodetector / LDR        │
│  GPIO Pin 8      │                │  GPIO Pin 7                 │
└──────────────────┘                └─────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| Transmitter (TX) | Accept serial text input, build protocol frames, Manchester-encode and transmit via laser | `tx/tx.ino` |
| Receiver (RX) | Sample and filter optical signal, detect start condition, Manchester-decode, validate and display frames | `rx/rx.ino` |
| Protocol Layer | Frame structure definition (preamble, IDs, sequence, length, payload, checksum, footer) | Embedded in both `.ino` files |
| Signal Filter | Majority-vote digital filter to reject noise on RX pin | `rx/rx.ino:15-23` (`readSignalFiltered()`) |
| Manchester Codec | Bit-level encoding (TX) and decoding (RX) of Manchester self-clocking code | `tx/tx.ino:27-34`, `rx/rx.ino:47-56` |

## Pattern Overview

**Overall:** Single-responsibility sketch pattern — each `.ino` file is a self-contained Arduino sketch responsible for exactly one role (transmit or receive).

**Key Characteristics:**
- No shared library or header files — protocol logic is duplicated between TX and RX sketches
- Cooperative single-threaded event loop (`setup()` / `loop()`)
- Hardcoded timing constants tuned for slow, reliable optical communication
- No dynamic memory allocation (fixed-size buffers: `msgBuffer[17]`, `payload[16]`)
- Polling-based serial input and GPIO sampling (no interrupts)

## Layers

**Application Layer:**
- Purpose: User-facing logic — serial I/O, message framing, protocol orchestration
- Location: `tx/tx.ino` (lines 67-86), `rx/rx.ino` (lines 58-101)
- Contains: `setup()`, `loop()`, `sendFrame()`, frame decoding logic
- Depends on: Protocol encoding/decoding layer, Hardware abstraction (Arduino Core)
- Used by: Direct user interaction via Serial Monitor

**Protocol Encoding/Decoding Layer:**
- Purpose: Manchester bit encoding, byte-level framing, checksum calculation, XOR encryption
- Location: `tx/tx.ino:21-65` (encoding), `rx/rx.ino:25-56` (decoding)
- Contains: `sendHalf()`, `sendManchesterBit()`, `sendByteManchester()`, `sampleHalfBit()`, `readByteManchester()`
- Depends on: Signal/timing layer
- Used by: Application layer

**Signal/Timing Layer:**
- Purpose: Precise microsecond-level timing for bit transmission and sampling, edge detection, noise filtering
- Location: `tx/tx.ino:17-19` (`waitUntil()`), `rx/rx.ino:15-45` (`readSignalFiltered()`, `waitEdgeStable()`, `waitUntil()`, `sampleHalfBit()`)
- Depends on: Arduino Core (`micros()`, `delayMicroseconds()`, `digitalRead()`)
- Used by: Protocol layer

**Hardware Abstraction Layer (Arduino Core):**
- Purpose: GPIO, timing, and serial I/O primitives
- Location: Arduino built-in libraries (not in repo)
- Contains: `pinMode()`, `digitalWrite()`, `digitalRead()`, `micros()`, `delayMicroseconds()`, `Serial`, `bitRead()`, `bitSet()`, `bitClear()`
- Depends on: AVR/ATmega hardware
- Used by: All layers above

## Data Flow

### Primary Request Path: TX Message Send

1. User types message into Serial Monitor (`tx/tx.ino:74-85` — `loop()` reads `Serial.available()`)
2. On newline, `sendFrame()` is called with sender ID, destination ID, sequence number, and message (`tx/tx.ino:80`)
3. `sendFrame()` builds the frame header, XOR-encrypts the payload, and computes the checksum (`tx/tx.ino:36-44`)
4. Start condition: laser HIGH for `START_HIGH_US` (800ms), then LOW for `START_OFF_US` (200ms) (`tx/tx.ino:46-53`)
5. Each byte is Manchester-encoded via `sendByteManchester()` → `sendManchesterBit()` → `sendHalf()` (`tx/tx.ino:55-60`)
6. `sendHalf()` sets laser pin and waits precisely `BIT_HALF_US` (200ms) using `waitUntil()` with `micros()` (`tx/tx.ino:21-25`)
7. End guard period of `END_GUARD_US` (500ms) with laser LOW (`tx/tx.ino:62-64`)
8. Serial confirms `">> Enviado."` (`tx/tx.ino:82`)

### Primary Request Path: RX Message Receive

1. `loop()` blocks until a HIGH edge is detected via `waitEdgeStable(true)` (`rx/rx.ino:65`)
2. Then blocks until the following LOW edge: `waitEdgeStable(false)` records `t0` (`rx/rx.ino:66`)
3. Start condition validated: HIGH duration must be ≥ `START_HIGH_MIN_US` (500ms) (`rx/rx.ino:68`)
4. Preamble byte (0xAA) decoded via Manchester with 15-sample majority-vote at bit center (`rx/rx.ino:71-72`)
5. ID byte decoded: upper nibble = source ID, lower nibble = destination ID (`rx/rx.ino:74`)
6. Sequence/Length byte decoded: length extracted via `len = sl & 0x0F` (`rx/rx.ino:75-76`)
7. Payload bytes decoded one by one into `payload[16]` array (`rx/rx.ino:80-81`)
8. Checksum and footer bytes decoded (`rx/rx.ino:82-83`)
9. Checksum validated by XOR of ID nibbles, length, and all payload bytes (`rx/rx.ino:85-86`)
10. If valid (`calc == rxChk && fin == 0x55`): payload XOR-decrypted and printed to Serial (`rx/rx.ino:89-92`)
11. If invalid: detailed debug output printed (expected vs actual checksum, footer, raw payload hex) (`rx/rx.ino:93-99`)

**State Management:**
- TX: `msgBuffer[17]` holds in-progress message; `msgLen` tracks current length; `seqNum` is a rolling 4-bit counter
- RX: `payload[16]` is a stack-allocated buffer reused each `loop()` iteration; no persistent state between frames

## Key Abstractions

**Manchester Encoding:**
- Purpose: Self-clocking line code where each bit has a mid-bit transition; 1 = HIGH→LOW, 0 = LOW→HIGH
- Examples: `tx/tx.ino:27-30` (`sendManchesterBit()`), `rx/rx.ino:47-56` (`readByteManchester()`)
- Pattern: Each bit is two half-bit periods; decoder samples both halves and expects complementary values

**Signal Filter (RX only):**
- Purpose: Reject transient noise on the photodetector pin by taking 20 rapid samples and using majority vote
- Examples: `rx/rx.ino:15-23` (`readSignalFiltered()`)
- Pattern: Read pin 20 times, count HIGHs, return `true` if >10 HIGHs; inverts if `INVERTED_LOGIC` is set

**Robust Bit Sampling (RX only):**
- Purpose: Sample each half-bit with 15 readings around the expected center to tolerate timing drift
- Examples: `rx/rx.ino:35-45` (`sampleHalfBit()`)
- Pattern: Position 10µs before center, take 15 samples spaced 500µs apart, majority-vote the result

**Frame Protocol (RFC-UNA-2026-PULSE):**
- Purpose: Define the over-the-air packet structure
- Examples: `tx/tx.ino:36-65` (`sendFrame()`), `rx/rx.ino:64-100` (frame decode in `loop()`)
- Pattern: `[START] [0xAA] [IDs] [Seq|Len] [Payload×N] [Checksum] [0x55] [END_GUARD]`

## Entry Points

**TX Sketch Entry:**
- Location: `tx/tx.ino:67-72` (`setup()`), `tx/tx.ino:74-86` (`loop()`)
- Triggers: Arduino boot → `setup()` once; then `loop()` called repeatedly
- Responsibilities: Initialize laser pin and Serial; accept user text input and send frames on newline

**RX Sketch Entry:**
- Location: `rx/rx.ino:58-62` (`setup()`), `rx/rx.ino:64-101` (`loop()`)
- Triggers: Arduino boot → `setup()` once; then `loop()` called repeatedly
- Responsibilities: Initialize sensor pin and Serial; continuously listen for and decode incoming frames

## Architectural Constraints

- **Threading:** Single-threaded cooperative event loop only. No concurrency, no interrupts used for communication. `waitUntil()` and `waitEdgeStable()` are blocking spin-loops.
- **Global state:** `seqNum` is a module-level counter in `tx/tx.ino:15`. `msgBuffer` and `msgLen` are module-level in `tx/tx.ino:13-14`. All RX state is stack-local within `loop()`.
- **Circular imports:** None — each `.ino` file is independently compiled as a separate sketch.
- **Memory:** Fixed-size buffers only (max 17 bytes for messages). No heap allocation (`malloc`/`new` not used). Suitable for ATmega328P's 2KB SRAM.
- **Timing precision:** Relies on `micros()` with `BIT_HALF_US = 200000` (200ms half-bits). Very slow by design — tolerant of significant timing jitter. Not suitable for high-speed communication.
- **Protocol duplication:** The frame format, XOR key, and Manchester logic are duplicated rather than shared between TX and RX sketches. Changes must be made in both files.

## Anti-Patterns

### Duplicated Protocol Logic

**What happens:** Protocol constants (`XOR_KEY`, `BIT_HALF_US`, frame structure) and encoding/decoding logic are copy-pasted between `tx/tx.ino` and `rx/rx.ino` rather than extracted into a shared header/library.
**Why it's wrong:** Any protocol change requires editing both files consistently. Divergence risk is high since there is no automated test or build that verifies compatibility.
**Do this instead:** Extract shared constants and encoding/decoding functions into a `common/protocol.h` header file, included by both sketches. This also enables unit testing the protocol layer independently of hardware.

### Blocking Spin-Loops

**What happens:** `waitUntil()` (`tx/tx.ino:17-19`, `rx/rx.ino:30-32`) and `waitEdgeStable()` (`rx/rx.ino:25-28`) use tight busy-wait loops that block all other processing.
**Why it's wrong:** While acceptable for this use case (single-purpose device), it prevents adding concurrent behaviors (e.g., blinking an activity LED, handling watchdog timers, or timeout-based error recovery during reception).
**Do this instead:** For future enhancements, consider using `millis()`-based state machines or timer interrupts for non-blocking timing. The current approach is acceptable given the simplicity of the system but limits extensibility.

### Hardcoded Magic Numbers

**What happens:** Protocol constants like `0xAA` (preamble), `0x55` (footer), `0x42` (XOR key), `0x0F` (bitmask) are used inline rather than as named constants in some places.
**Why it's wrong:** Reduces readability and makes protocol changes error-prone (e.g., changing preamble requires finding every occurrence).
**Do this instead:** Define all protocol constants as named `const` values in a shared header. This is partially done (timing constants and XOR key are named) but preamble/footer/bitmasks are inline magic numbers.

## Error Handling

**Strategy:** Validation with silent discard on failure; detailed debug output on RX.

**Patterns:**
- TX: No error handling — assumes hardware always works
- RX frame validation: Checks start pulse duration (`rx/rx.ino:68`), preamble value (`rx/rx.ino:72`), payload length bounds (`rx/rx.ino:78`), checksum (`rx/rx.ino:89`), and footer (`rx/rx.ino:89`). Invalid frames are silently discarded after debug output.
- RX debug output: On checksum/footer failure, prints expected vs actual checksum, footer, and raw payload hex to Serial (`rx/rx.ino:93-99`)
- No retry mechanism, no ACK/NACK, no timeout on partial frames

## Cross-Cutting Concerns

**Logging:** Serial output at 9600 baud. TX logs status messages; RX logs received messages and frame validation failures with detailed hex dumps.

**Validation:** Frame-level only (preamble, length range, checksum, footer). No message-level validation beyond length check.

**Authentication:** None. Device identity is a hardcoded 4-bit ID with no cryptographic verification. XOR key `0x42` provides trivial payload obfuscation only.

---

*Architecture analysis: 2026-05-10*
