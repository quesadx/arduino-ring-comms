// === RFC-UNA-2026-PULSE: RECEPTOR (RX) - FINAL CON FILTRO ===
const int RX_PIN = 7;
const uint8_t XOR_KEY  = 0x42;
const uint8_t MY_ID    = 0x2;

const uint32_t BIT_HALF_US       = 200000UL; 
const uint32_t CENTER_OFFSET_US  = 100000UL; 
const uint32_t START_OFF_US      = 200000UL; 
const uint32_t START_HIGH_MIN_US = 500000UL; 

// IMPORTANTE: Ajustar segun el analisis de captura
const bool INVERTED_LOGIC = true; 

// Lee el pin 20 veces y toma la mayoría para eliminar ruido de microsegundos
bool readSignalFiltered() {
  int highCount = 0;
  for(int i = 0; i < 20; i++) {
    bool raw = digitalRead(RX_PIN);
    if (INVERTED_LOGIC) raw = !raw;
    if (raw) highCount++;
  }
  return highCount > 10;
}

uint32_t waitEdgeStable(bool level) {
  while (readSignalFiltered() != level);
  return micros();
}

void waitUntil(uint32_t target) {
  while ((long)(micros() - target) < 0);
}

// Muestreo robusto: toma 15 muestras en el centro del semiciclo
bool sampleHalfBit(uint32_t t0, uint8_t index) {
  uint32_t center = t0 + START_OFF_US + CENTER_OFFSET_US + ((uint32_t)index * BIT_HALF_US);
  waitUntil(center - 10000); // 10ms antes del centro
  
  int accum = 0;
  for(int i = 0; i < 15; i++) {
    if(readSignalFiltered()) accum++;
    delayMicroseconds(500); 
  }
  return accum > 7;
}

uint8_t readByteManchester(uint32_t t0, uint8_t &idx) {
  uint8_t b = 0;
  for (int i = 7; i >= 0; i--) {
    bool f = sampleHalfBit(t0, idx++);
    bool s = sampleHalfBit(t0, idx++);
    if (f && !s) bitSet(b, i); 
    else if (!f && s) bitClear(b, i);
  }
  return b;
}

void setup() {
  pinMode(RX_PIN, INPUT);
  Serial.begin(9600);
  Serial.println("RX Listo. Esperando laser...");
}

void loop() {
  uint32_t tHigh = waitEdgeStable(true);
  uint32_t t0    = waitEdgeStable(false);

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
  uint8_t fin = readByteManchester(t0, idx);

  uint8_t calc = ((ids >> 4) & 0x0F) ^ (ids & 0x0F) ^ ((sl >> 4) & 0x0F) ^ len;
  for (int i = 0; i < len; i++) calc ^= payload[i];

// Sustituye la parte final del loop por esto para debuguear:
  if (calc == rxChk && fin == 0x55) {
    Serial.print(">> [OK] Msg: ");
    for (int i = 0; i < len; i++) Serial.write(payload[i] ^ XOR_KEY);
    Serial.println();
  } else {
    Serial.print(">> [FALLO] Calc: 0x"); Serial.print(calc, HEX);
    Serial.print(" RX_Chk: 0x"); Serial.print(rxChk, HEX);
    Serial.print(" Fin: 0x"); Serial.println(fin, HEX);
    Serial.print(" Payload Recibido: ");
    for (int i = 0; i < len; i++) { Serial.print(payload[i], HEX); Serial.print(" "); }
    Serial.println();
  }
}