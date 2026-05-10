# Codebase Structure

**Analysis Date:** 2026-05-10

## Directory Layout

```
arduino-ring-comms/
├── tx/                    # Transmitter Arduino sketch
│   └── tx.ino             #   TX main sketch (86 lines)
├── rx/                    # Receiver Arduino sketch
│   └── rx.ino             #   RX main sketch (101 lines)
└── .planning/             # Planning artifacts (generated, not source)
    └── codebase/          #   Codebase maps (this directory)
```

## Directory Purposes

**`tx/`:**
- Purpose: Transmitter sketch — accepts typed messages via Serial, encodes them using the RFC-UNA-2026-PULSE protocol (Manchester encoding, XOR encryption), and transmits via laser on GPIO pin 8
- Contains: Single `.ino` sketch file
- Key files: `tx/tx.ino`

**`rx/`:**
- Purpose: Receiver sketch — reads photodetector on GPIO pin 7, applies digital signal filtering, decodes Manchester-encoded frames, validates checksums, decrypts payload via XOR, and outputs messages to Serial
- Contains: Single `.ino` sketch file
- Key files: `rx/rx.ino`

## Key File Locations

**Entry Points:**
- `tx/tx.ino:67-72` (`setup()`), `tx/tx.ino:74-86` (`loop()`): TX sketch entry
- `rx/rx.ino:58-62` (`setup()`), `rx/rx.ino:64-101` (`loop()`): RX sketch entry

**Configuration:**
- `tx/tx.ino:2-11`: TX hardware configuration (pin, IDs, XOR key, timing constants)
- `rx/rx.ino:2-12`: RX hardware configuration (pin, IDs, XOR key, timing constants, inverted logic flag)

**Core Logic:**
- `tx/tx.ino:36-65` (`sendFrame()`): Frame construction and transmission
- `tx/tx.ino:21-34`: Manchester encoding functions (`sendHalf()`, `sendManchesterBit()`, `sendByteManchester()`)
- `rx/rx.ino:47-56` (`readByteManchester()`): Manchester byte decoding
- `rx/rx.ino:35-45` (`sampleHalfBit()`): Robust half-bit sampling with majority vote
- `rx/rx.ino:15-23` (`readSignalFiltered()`): Digital signal noise filter
- `rx/rx.ino:25-28` (`waitEdgeStable()`): Edge detection with filtering
- `rx/rx.ino:64-101` (RX `loop()`): Full frame receive and validation pipeline

**Testing:**
- No test files present

## Naming Conventions

**Files:**
- Lowercase directory names match sketch purpose (`tx/`, `rx/`)
- `.ino` sketch files match directory name (Arduino convention: sketch folder and `.ino` file must share the same name)

**Directories:**
- Short, lowercase, descriptive names (`tx`, `rx`)

**Constants (C++):**
- `UPPER_SNAKE_CASE` for timing constants: `BIT_HALF_US`, `START_HIGH_US`, `END_GUARD_US`
- `UPPER_SNAKE_CASE` for pin definitions: `LASER_PIN`, `RX_PIN`
- `UPPER_SNAKE_CASE` for protocol constants: `XOR_KEY`, `MY_ID`, `DEST_ID`

**Functions:**
- `camelCase`: `readSignalFiltered()`, `waitEdgeStable()`, `sampleHalfBit()`, `sendManchesterBit()`, `sendByteManchester()`, `readByteManchester()`, `sendFrame()`

**Variables:**
- `camelCase` for locals and module-level: `msgBuffer`, `msgLen`, `seqNum`, `highCount`, `tHigh`, `t0`
- Single-purpose short names acceptable in tight loops: `f`, `s`, `b`, `i`, `c`

## Where to Add New Code

**New Feature (e.g., new protocol variant, new encoding):**
- If TX-only logic: add functions to `tx/tx.ino`
- If RX-only logic: add functions to `rx/rx.ino`
- If shared protocol logic: create a new `common/` directory at project root with a `.h` header file (e.g., `common/protocol.h`), then `#include` from both `.ino` files

**New Component/Module (e.g., status LED, buzzer feedback):**
- Implementation: add new `.ino` sketch in a new directory, or add functions to the existing TX/RX sketches if the component is bound to one role

**Utilities (shared helpers):**
- Shared helpers: create `common/utils.h` (or `utils.h`) in a new `common/` directory
- Currently no shared code exists — all utilities are duplicated between TX and RX

**Tests:**
- No existing test infrastructure. If adding tests, use the ArduinoUnit or AUnit framework
- Place test sketches in a `tests/` directory at project root

## Special Directories

**`.planning/`:**
- Purpose: GSD planning artifacts — codebase maps, implementation plans, phase definitions
- Generated: Yes (by GSD tooling)
- Committed: Yes (planning artifacts are tracked in git)

---

*Structure analysis: 2026-05-10*
