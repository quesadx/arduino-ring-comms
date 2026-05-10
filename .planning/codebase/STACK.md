# Technology Stack

**Analysis Date:** 2026-05-10

## Languages

**Primary:**
- C++ (Arduino Wiring subset) - All application logic in `.ino` sketch files

## Runtime

**Environment:**
- Arduino Core (AVR or compatible) - Provides `setup()`/`loop()` lifecycle, `Serial`, GPIO, and timing primitives
- Target board: Arduino-compatible (Uno, Nano, or similar ATmega-based board)
- No RTOS or threading — single-threaded cooperative event loop

**Package Manager:**
- Not applicable — no external package manager used. Arduino IDE manages board packages and core libraries internally.

## Frameworks

**Core:**
- Arduino Wiring Framework - Built-in functions: `digitalRead()`, `digitalWrite()`, `pinMode()`, `micros()`, `delayMicroseconds()`, `bitRead()`, `bitSet()`, `bitClear()`, `Serial`

**Testing:**
- None — no test framework or test files present

**Build/Dev:**
- Arduino IDE (inferred) — `.ino` sketches compiled via Arduino CLI or IDE
- No build configuration files present (no `platformio.ini`, no `Makefile`, no `CMakeLists.txt`)

## Key Dependencies

**Critical:**
- No external libraries or third-party dependencies. All functionality uses Arduino core built-ins only.

**Infrastructure:**
- None — no database, no external services, no cloud platforms

## Configuration

**Environment:**
- No `.env` files, no configuration files of any kind
- All configuration is hardcoded as `const` values within each `.ino` sketch

**Build:**
- No build configuration files present (`platformio.ini`, `Makefile`, `CMakeLists.txt`, `package.json` all absent)

## Platform Requirements

**Development:**
- Arduino IDE or Arduino CLI
- USB connection to Arduino board for sketch upload
- Serial monitor at 9600 baud for interaction

**Production:**
- Arduino-compatible board (ATmega328P or similar)
- Laser module connected to digital pin 8 (TX) / photodetector on digital pin 7 (RX)
- Serial connection at 9600 baud for TX message input and RX message output

---

*Stack analysis: 2026-05-10*
