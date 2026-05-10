// === RFC-UNA-2026-PULSE: TRANSCEPTOR (TX+RX) - SINGLE BOARD ===
// Combines transmitter and receiver on one Arduino.
// D8 = laser output, D7 = photodetector input.

// -------------------- Hardware Configuration --------------------
const int TX_PIN        = 8;
const int RX_PIN        = 7;
const uint8_t XOR_KEY   = 0x42;
const uint8_t MY_ID     = 0x1;
const uint8_t DEST_ID   = 0x2;

// -------------------- Timing Constants --------------------
const uint32_t BIT_HALF_US       = 200000UL;   // 200ms half-bit
const uint32_t START_HIGH_US     = 800000UL;   // 800ms start pulse HIGH
const uint32_t START_OFF_US      = 200000UL;   // 200ms start pulse LOW
const uint32_t END_GUARD_US      = 500000UL;   // 500ms end guard
const uint32_t CENTER_OFFSET_US  = 100000UL;   // sample center offset
const uint32_t START_HIGH_MIN_US = 500000UL;   // minimum valid start HIGH

// Photodetector logic inversion (adjust based on sensor)
const bool INVERTED_LOGIC = true;

// --- Calibration / debug pins ---
const int BUTTON_PIN    = 2;    // on-board or external button (uses INPUT_PULLUP)
const int RX_LED_PIN    = 13;   // built-in LED to indicate RX for calibration
// Set to true to run in simple calibration mode: print sensor output and
// let the button turn the RX LED on. Disable to restore normal transceiver.
const bool CALIBRATE_MODE = true;
const int ANALOG_RX_PIN = A0;   // optional analog read of sensor for thresholding
// RX wait timeout — prevents blocking Serial input forever
const uint32_t RX_WAIT_TIMEOUT_US = 3000000UL;  // 3s max wait for edge

// -------------------- TX State --------------------
char     msgBuffer[17];
int      msgLen   = 0;
uint8_t  seqNum   = 0;
bool     txBusy   = false;   // guard: skip RX checks while transmitting

// -------------------- Shared Timing Helper --------------------
void waitUntil(uint32_t target) {
  while ((long)(micros() - target) < 0);
}

// ==================== TRANSMITTER FUNCTIONS ====================

void sendHalf(bool level, uint32_t& t_next) {
  digitalWrite(TX_PIN, level ? HIGH : LOW);
  t_next += BIT_HALF_US;
  waitUntil(t_next);
}

void sendManchesterBit(bool val, uint32_t& t_next) {
  sendHalf(val,  t_next);   // Manchester: 1 = H→L, 0 = L→H
  sendHalf(!val, t_next);
}

void sendByteManchester(uint8_t b, uint32_t& t_next) {
  for (int i = 7; i >= 0; i--) sendManchesterBit(bitRead(b, i), t_next);
}

void sendFrame(uint8_t id_o, uint8_t id_d, uint8_t seq, const char* msg) {
  uint8_t len = strlen(msg);
  uint8_t payload[16];
  uint8_t chk = id_o ^ id_d ^ seq ^ len;

  for (int i = 0; i < len; i++) {
    payload[i] = msg[i] ^ XOR_KEY;
    chk ^= payload[i];
  }

  uint32_t t_next = micros();

  // Start condition: HIGH then LOW
  digitalWrite(TX_PIN, HIGH);
  t_next += START_HIGH_US;
  waitUntil(t_next);

  digitalWrite(TX_PIN, LOW);
  t_next += START_OFF_US;
  waitUntil(t_next);

  // Frame body
  sendByteManchester(0xAA, t_next);                              // preamble
  sendByteManchester((id_o << 4) | id_d, t_next);                // IDs
  sendByteManchester((seq << 4) | len, t_next);                  // seq + length
  for (int i = 0; i < len; i++) sendByteManchester(payload[i], t_next);
  sendByteManchester(chk, t_next);                               // checksum
  sendByteManchester(0x55, t_next);                              // footer

  // End guard
  digitalWrite(TX_PIN, LOW);
  t_next += END_GUARD_US;
  waitUntil(t_next);
}

// ==================== RECEIVER FUNCTIONS ====================

bool readSignalFiltered() {
  int highCount = 0;
  for (int i = 0; i < 20; i++) {
    bool raw = digitalRead(RX_PIN);
    if (INVERTED_LOGIC) raw = !raw;
    if (raw) highCount++;
  }
  return highCount > 10;
}

// Debug helper: return raw high-count (no filtering/inversion)
int debugReadSensorRaw() {
  int highCount = 0;
  for (int i = 0; i < 20; i++) {
    bool raw = digitalRead(RX_PIN);
    if (raw) highCount++;
    delayMicroseconds(50);
  }
  return highCount;
}

// waitEdge with timeout — returns true if edge found, false on timeout
bool waitEdgeStable(bool level, uint32_t& t_edge) {
  uint32_t start = micros();
  while (readSignalFiltered() != level) {
    if ((long)(micros() - start) >= (long)RX_WAIT_TIMEOUT_US) return false;
  }
  t_edge = micros();
  return true;
}

bool sampleHalfBit(uint32_t t0, uint8_t index) {
  uint32_t center = t0 + START_OFF_US + CENTER_OFFSET_US + ((uint32_t)index * BIT_HALF_US);
  waitUntil(center - 10000);  // 10ms before center

  int accum = 0;
  for (int i = 0; i < 15; i++) {
    if (readSignalFiltered()) accum++;
    delayMicroseconds(500);
  }
  return accum > 7;
}

uint8_t readByteManchester(uint32_t t0, uint8_t &idx) {
  uint8_t b = 0;
  for (int i = 7; i >= 0; i--) {
    bool f = sampleHalfBit(t0, idx++);
    bool s = sampleHalfBit(t0, idx++);
    if (f && !s)      bitSet(b, i);
    else if (!f && s) bitClear(b, i);
  }
  return b;
}

// ==================== RX FRAME DECODER ====================

void receiveFrame() {
  uint32_t tHigh;
  if (!waitEdgeStable(true, tHigh)) return;

  uint32_t t0;
  if (!waitEdgeStable(false, t0)) return;

  if ((t0 - tHigh) < START_HIGH_MIN_US) return;

  uint8_t idx = 0;
  uint8_t preamble = readByteManchester(t0, idx);
  if (preamble != 0xAA) return;

  uint8_t ids = readByteManchester(t0, idx);
  uint8_t sl  = readByteManchester(t0, idx);
  uint8_t len = sl & 0x0F;

  if (len == 0 || len > 16) return;

  uint8_t payload[16];
  for (int i = 0; i < len; i++) payload[i] = readByteManchester(t0, idx);
  uint8_t rxChk = readByteManchester(t0, idx);
  uint8_t fin   = readByteManchester(t0, idx);

  uint8_t calc = ((ids >> 4) & 0x0F) ^ (ids & 0x0F) ^ ((sl >> 4) & 0x0F) ^ len;
  for (int i = 0; i < len; i++) calc ^= payload[i];

  if (calc == rxChk && fin == 0x55) {
    Serial.print("[RX] ");
    for (int i = 0; i < len; i++) Serial.write(payload[i] ^ XOR_KEY);
    Serial.println();
  } else {
    Serial.print("[RX:FALLO] Calc:0x"); Serial.print(calc, HEX);
    Serial.print(" RxChk:0x"); Serial.print(rxChk, HEX);
    Serial.print(" Fin:0x"); Serial.print(fin, HEX);
    Serial.print(" Payload:");
    for (int i = 0; i < len; i++) { Serial.print(" "); Serial.print(payload[i], HEX); }
    Serial.println();
  }
}

// ==================== TX MESSAGE HANDLER ====================

void handleSerialInput() {
  if (!Serial.available()) return;

  char c = Serial.read();
  if (c == '\n' || c == '\r') {
    if (msgLen > 0) {
      msgBuffer[msgLen] = '\0';
      txBusy = true;
      sendFrame(MY_ID, DEST_ID, seqNum++ & 0x0F, msgBuffer);
      txBusy = false;
      msgLen = 0;
      Serial.println("[TX] Enviado.");
    }
  } else if (msgLen < 16 && c >= 32) {
    msgBuffer[msgLen++] = c;
  }
}

// ==================== SETUP / LOOP ====================

void setup() {
  if (!CALIBRATE_MODE) {
    pinMode(TX_PIN, OUTPUT);
    digitalWrite(TX_PIN, LOW);
  }
  pinMode(RX_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RX_LED_PIN, OUTPUT);
  Serial.begin(9600);
  if (CALIBRATE_MODE) Serial.println("CALIBRATE: raw sensor read only (no TX/filters)");
  else Serial.println("TRANSCEPTOR Listo. Escribe mensaje (TX) o espera laser (RX)...");
}

void loop() {
  if (CALIBRATE_MODE) {
    // Calibration/debug mode: print raw sensor output periodically
    static unsigned long lastMs = 0;
    // Button pressed -> force RX LED on for visual calibration
    bool btnPressed = digitalRead(BUTTON_PIN) == LOW;
    digitalWrite(RX_LED_PIN, btnPressed ? HIGH : LOW);

    if (millis() - lastMs >= 200) {
      lastMs = millis();
      int rawCount = debugReadSensorRaw();
      int analogVal = analogRead(ANALOG_RX_PIN);
      Serial.print("SENSOR rawCount="); Serial.print(rawCount);
      Serial.print(" analog="); Serial.println(analogVal);
    }
    // Skip normal transceiver behavior while in calibration mode
    return;
  }

  // 1. Always handle serial input first (non-blocking)
  handleSerialInput();

  // 2. If not transmitting, listen for incoming frames
  //    (receiveFrame returns quickly when no signal via timeouts)
  if (!txBusy) {
    receiveFrame();
  }
}
