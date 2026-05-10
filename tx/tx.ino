// === RFC-UNA-2026-PULSE: EMISOR (TX) - FINAL ===
const int LASER_PIN = 8;
const uint8_t XOR_KEY  = 0x42;
const uint8_t MY_ID    = 0x1;
const uint8_t DEST_ID  = 0x2;

// Tiempos para hardware lento
const uint32_t BIT_HALF_US   = 200000UL; // 200ms
const uint32_t START_HIGH_US = 800000UL; // 800ms
const uint32_t START_OFF_US  = 200000UL; // 200ms
const uint32_t END_GUARD_US  = 500000UL; 

char msgBuffer[17];
int msgLen = 0;
uint8_t seqNum = 0;

void waitUntil(uint32_t target) {
  while ((long)(micros() - target) < 0);
}

void sendHalf(bool level, uint32_t& t_next) {
  digitalWrite(LASER_PIN, level ? HIGH : LOW);
  t_next += BIT_HALF_US;
  waitUntil(t_next);
}

void sendManchesterBit(bool val, uint32_t& t_next) {
  sendHalf(val,  t_next); // Manchester: 1=H->L, 0=L->H
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
  digitalWrite(LASER_PIN, HIGH);
  t_next += START_HIGH_US;
  waitUntil(t_next);
  
  digitalWrite(LASER_PIN, LOW);
  t_next += START_OFF_US;
  waitUntil(t_next);

  sendByteManchester(0xAA, t_next);
  sendByteManchester((id_o << 4) | id_d, t_next);
  sendByteManchester((seq << 4) | len, t_next);
  for (int i = 0; i < len; i++) sendByteManchester(payload[i], t_next);
  sendByteManchester(chk, t_next);
  sendByteManchester(0x55, t_next);

  digitalWrite(LASER_PIN, LOW);
  t_next += END_GUARD_US;
  waitUntil(t_next);
}

void setup() {
  pinMode(LASER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, LOW);
  Serial.begin(9600);
  Serial.println("TX Listo. Escribe mensaje:");
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (msgLen > 0) {
        msgBuffer[msgLen] = '\0';
        sendFrame(MY_ID, DEST_ID, seqNum++ & 0x0F, msgBuffer);
        msgLen = 0;
        Serial.println(">> Enviado.");
      }
    } else if (msgLen < 16 && c >= 32) msgBuffer[msgLen++] = c;
  }
}