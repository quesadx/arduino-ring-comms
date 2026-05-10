// ==================================================================
//   TRANSCEPTOR — PULSE (TX) + ULNET 4-bit (RX) — Single Arduino
// ==================================================================
// Sends in RFC-UNA-2026-PULSE format, receives in RFC-UNA-2026-ULNET XA
// format with 4-bit source/destination addresses.
//
// Hardware:
//   D2  = Boton (a GND, INPUT_PULLUP) — opcional, para debug
//   D8  = Laser / LED (TX)
//   A0  = Pin S del modulo LDR (sensor analogico de luz)
//   D13 = LED integrado (indicador RX, opcional)
//
// Umbral de luz: analogRead ≤ 75 → luz detectada (1), > 75 → oscuridad (0)

// -------------------- Hardware --------------------
const int TX_PIN       = 8;
const int RX_ANALOG    = A0;
const int BTN_PIN      = 2;
const int LED_PIN      = 13;

// -------------------- Nuestro ID --------------------
const uint8_t MY_ID    = 0x1;

// ==================== PROTOCOLO PULSE (TX — rfc.md) ====================
// Manchester, 50 ms/bit = 25 ms half-bit
const uint32_t PULSE_HALF_US   =  25000UL;
const uint32_t PULSE_START_HI  = 150000UL;  // 150 ms HIGH
const uint32_t PULSE_START_LO  =  50000UL;  //  50 ms LOW
const uint32_t PULSE_END_GUARD = 100000UL;  // 100 ms LOW after frame
const uint8_t  PULSE_PREAMBLE  = 0xAA;
const uint8_t  PULSE_FOOTER    = 0x55;
const uint8_t  PULSE_XOR_KEY   = 0x42;

// ==================== PROTOCOLO ULNET (RX — rfc-that-we-recieve.md) ====================
// Simple on/off, 100 ms/bit, direcciones de 4 bits
const uint32_t ULNET_BIT_US    = 100000UL;  // 100 ms
const uint16_t ULNET_THRESHOLD = 75;        // analog ≤ 75 = luz
const uint8_t  ULNET_PREAMBLE  = 0xFE;      // 11111110
const uint8_t  ULNET_XOR_KEY   = 0x5A;
const uint8_t  ULNET_BROADCAST = 0x0F;      // broadcast address

// Tiempo entre tramas ULNET (el emisor manda 400 ms LOW)
const uint32_t ULNET_GAP_US    = 400000UL;

// Maximo de espera para borde de subida sin quedarnos bloqueados
const uint32_t EDGE_TIMEOUT_US = 5000000UL;  // 5 s

// -------------------- Estado TX (PULSE) --------------------
char    msgBuffer[17];
int     msgLen  = 0;
uint8_t seqNum  = 0;
bool    txBusy  = false;

// -------------------- Utilidad compartida --------------------
void waitUntil(uint32_t target) {
  while ((long)(micros() - target) < 0);
}

// ==================== TX: PULSE (Manchester) ====================

void sendHalf(bool level, uint32_t& t_next) {
  digitalWrite(TX_PIN, level ? HIGH : LOW);
  t_next += PULSE_HALF_US;
  waitUntil(t_next);
}

void sendManchesterBit(bool val, uint32_t& t_next) {
  sendHalf( val, t_next);   // 1 = H→L, 0 = L→H
  sendHalf(!val, t_next);
}

// Envia n bits Manchester MSB-first (n ≤ 8)
void sendManchesterBits(uint8_t val, uint8_t n, uint32_t& t_next) {
  for (int i = n - 1; i >= 0; i--)
    sendManchesterBit(bitRead(val, i), t_next);
}

// Frame PULSE completo segun rfc.md
void sendFrame_PULSE(uint8_t id_orig, uint8_t id_dest, uint8_t seq,
                     const char* msg) {
  uint8_t len = strlen(msg);
  uint8_t payload[16];
  uint8_t chk = id_orig ^ id_dest ^ seq ^ len;

  for (int i = 0; i < len; i++) {
    payload[i] = msg[i] ^ PULSE_XOR_KEY;
    chk ^= payload[i];
  }

  digitalWrite(LED_PIN, HIGH);  // indicador visual

  uint32_t t_next = micros();

  // Start pulse: HIGH 150ms, LOW 50ms
  digitalWrite(TX_PIN, HIGH);
  t_next += PULSE_START_HI;
  waitUntil(t_next);
  digitalWrite(TX_PIN, LOW);
  t_next += PULSE_START_LO;
  waitUntil(t_next);

  // Cuerpo de la trama PULSE
  sendManchesterBits(PULSE_PREAMBLE, 8, t_next);   // 0xAA
  sendManchesterBits(id_orig,         4, t_next);   // origen
  sendManchesterBits(id_dest,         4, t_next);   // destino
  sendManchesterBits(seq,             4, t_next);   // secuencia
  sendManchesterBits(len,             8, t_next);   // longitud
  for (int i = 0; i < len; i++)
    sendManchesterBits(payload[i],    8, t_next);   // payload cifrado
  sendManchesterBits(chk,             8, t_next);   // checksum
  sendManchesterBits(PULSE_FOOTER,    8, t_next);   // 0x55

  // End guard
  digitalWrite(TX_PIN, LOW);
  t_next += PULSE_END_GUARD;
  waitUntil(t_next);

  digitalWrite(LED_PIN, LOW);
}

// ==================== RX: ULNET (on/off simple) ====================

// Lee el sensor analogico y devuelve true si hay luz (≤ umbral)
bool readLight() {
  // Promedio de 3 lecturas para filtrar ruido
  long sum = 0;
  for (int i = 0; i < 3; i++) {
    sum += analogRead(RX_ANALOG);
    delayMicroseconds(200);
  }
  return (sum / 3) <= ULNET_THRESHOLD;
}

// Espera un borde de subida (transicion oscuridad→luz).
// Devuelve true y guarda t0 si lo encuentra antes del timeout.
bool waitRisingEdge(uint32_t& t0) {
  uint32_t start = micros();
  // Primero esperar a que este oscuro (por si ya hay luz residual)
  while (readLight()) {
    if ((long)(micros() - start) >= (long)EDGE_TIMEOUT_US) return false;
  }
  // Ahora esperar el borde de subida
  while (!readLight()) {
    if ((long)(micros() - start) >= (long)EDGE_TIMEOUT_US) return false;
  }
  t0 = micros();
  return true;
}

// Formatea una trama ULNET recibida para mostrarla en Serial
void printULNETFrame(uint8_t dest, uint8_t orig, uint8_t dato, uint8_t chk) {
  Serial.print(F("[ULNET] Dest:0x")); Serial.print(dest, HEX);
  Serial.print(F(" Orig:0x"));         Serial.print(orig, HEX);
  Serial.print(F(" Data:0x"));         Serial.print(dato, HEX);
  Serial.print(F(" Chk:0x"));          Serial.print(chk, HEX);
}

// Recibe y decodifica una trama ULNET.
// Retorna true si la trama fue valida y procesada.
// El puntero relay se activa si la trama no es para nosotros
// y debemos reenviarla en formato PULSE.
bool receiveFrame_ULNET() {
  // Esperar borde de subida
  uint32_t t0;
  if (!waitRisingEdge(t0)) return false;

  // Leer 31 bits, muestreando en el centro de cada periodo (100ms)
  // Sample[i] en t0 + 50ms + i*100ms, i=0..30
  uint32_t frame = 0;  // max 31 bits, cabe en uint32_t
  for (uint8_t i = 0; i < 31; i++) {
    uint32_t t_sample = t0 + 50000UL + ((uint32_t)i * ULNET_BIT_US);
    waitUntil(t_sample);
    if (readLight())
      frame |= ((uint32_t)1 << (30 - i));  // MSB primero → pos 30 es el 1er bit
  }

  // Extraer campos (MSB-first dentro del frame de 31 bits)
  uint8_t preamble = (frame >> 23) & 0xFF;  // bits 30..23
  if (preamble != ULNET_PREAMBLE) return false;

  uint8_t dest    = (frame >> 19) & 0x0F;   // bits 22..19
  uint8_t orig    = (frame >> 15) & 0x0F;   // bits 18..15
  uint8_t len     = (frame >> 10) & 0x1F;   // bits 14..10
  uint8_t data    = (frame >>  5) & 0x1F;   // bits  9..5
  uint8_t chk     = (frame >>  0) & 0x1F;   // bits  4..0

  // Verificar checksum (en ULNET chk == data cifrada)
  if (chk != data) {
    Serial.print(F("[RX:ULNET] Checksum fail — "));
    printULNETFrame(dest, orig, data, chk);
    Serial.println();
    return false;
  }

  // Descifrar dato
  uint8_t decrypted = data ^ ULNET_XOR_KEY;
  char ascii = 'a' + (decrypted & 0x1F);

  // ¿Es para nosotros?
  if (dest == MY_ID || dest == ULNET_BROADCAST) {
    Serial.print(F("[RX:OK] "));
    printULNETFrame(dest, orig, data, chk);
    Serial.print(F(" -> '"));
    Serial.print(ascii);
    Serial.println(F("'"));
    return true;
  }

  // No es para nosotros → reenviar en formato PULSE
  Serial.print(F("[RX:RELAY] "));
  printULNETFrame(dest, orig, data, chk);
  Serial.print(F(" -> reenviando '"));
  Serial.print(ascii);
  Serial.println(F("'"));

  // Empaquetar en trama PULSE y enviar (ID origen/destino preservados)
  char payload[2] = { ascii, '\0' };
  txBusy = true;
  sendFrame_PULSE(orig, dest, seqNum++ & 0x0F, payload);
  txBusy = false;

  Serial.println(F("[TX:RELAY] Trama PULSE reenviada."));
  return true;
}

// ==================== Manejo de entrada Serial ====================

void handleSerialInput() {
  if (!Serial.available()) return;

  char c = Serial.read();
  if (c == '\n' || c == '\r') {
    if (msgLen > 0) {
      msgBuffer[msgLen] = '\0';

      uint8_t dest;
      if (msgBuffer[0] == '@') {
        // Formato: @X mensaje  — destino especificado
        dest = msgBuffer[1] - '0';
        if (dest > 9) dest = msgBuffer[1] - 'A' + 10;
        if (dest > 15) dest = 0x02;  // default
        // Quitar "@X" del mensaje
        for (int i = 0; i <= msgLen - 3; i++)
          msgBuffer[i] = msgBuffer[i + 2];
        msgLen -= 2;
        msgBuffer[msgLen] = '\0';
      } else {
        dest = 0x02;  // destino por defecto
      }

      txBusy = true;
      sendFrame_PULSE(MY_ID, dest & 0x0F, seqNum++ & 0x0F, msgBuffer);
      txBusy = false;
      msgLen = 0;

      Serial.print(F("[TX:PULSE] Enviado a 0x"));
      Serial.println(dest & 0x0F, HEX);
    }
  } else if (msgLen < 16 && c >= 32) {
    msgBuffer[msgLen++] = c;
  }
}

// ==================== Setup / Loop ====================

void setup() {
  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, LOW);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(9600);
  Serial.println(F("======================================"));
  Serial.println(F(" TRANSCEPTOR — PULSE(TX) + ULNET(RX)"));
  Serial.print  (F(" Mi ID: 0x")); Serial.println(MY_ID, HEX);
  Serial.println(F(" TX: PULSE (Manchester, 50ms/bit)"));
  Serial.println(F(" RX: ULNET (100ms/bit, umbral=75)"));
  Serial.println(F(" Escribe mensaje o espera laser..."));
  Serial.println(F(" Destino: @X al inicio del mensaje"));
  Serial.println(F("   ej: \"@3 hola\" envia a nodo 3"));
  Serial.println(F("======================================"));
}

void loop() {
  // 1. Procesar entrada serial (no bloqueante)
  handleSerialInput();

  // 2. Si no estamos transmitiendo, escuchar tramas ULNET
  if (!txBusy) {
    receiveFrame_ULNET();

    // Pequeña pausa para no saturar el ADC
    delay(1);
  }
}
