// ==================================================================
//   TRANSCEPTOR — PULSE (TX) + Steven (RX) — Single Arduino
// ==================================================================
// TX: RFC-UNA-2026-PULSE (rfc.md) — Manchester, 50ms/bit, XOR 0x42
// RX: Steven protocol — pulse-width, 100/300ms bits, XOR 0x5A
//
// Hardware: D8=laser, A0=sensor LDR, D2=boton, D13=LED

// -------------------- Hardware --------------------
const int TX_PIN       = 8;
const int RX_ANALOG    = A0;
const int BTN_PIN      = 2;
const int LED_PIN      = 13;

// -------------------- Nuestro ID --------------------
const uint8_t MY_ID    = 0x1;

// -------------------- Umbral de luz (del debug: ≤75 = laser) --------------------
const uint16_t LIGHT_THRESHOLD = 75;

// ==================== PROTOCOLO PULSE (TX — rfc.md) ====================
const uint32_t PULSE_HALF_US   =  25000UL;
const uint32_t PULSE_START_HI  = 150000UL;
const uint32_t PULSE_START_LO  =  50000UL;
const uint32_t PULSE_END_GUARD = 100000UL;
const uint8_t  PULSE_PREAMBLE  = 0xAA;
const uint8_t  PULSE_FOOTER    = 0x55;
const uint8_t  PULSE_XOR_KEY   = 0x42;

// ==================== PROTOCOLO Steven (RX) ====================
// Steven encoding: START=500ms, bit0=100ms, bit1=300ms, FIN=700ms
// Each pulse followed by 100ms LOW pause
const uint32_t STEVEN_THR_FIN   = 600000UL;  // >600ms → FIN
const uint32_t STEVEN_THR_START = 400000UL;  // >400ms → START
const uint32_t STEVEN_THR_BIT1  = 200000UL;  // >200ms → bit 1
// ≤200ms → bit 0

const uint8_t STEVEN_XOR_KEY   = 0x5A;

// Timeout for edge detection per loop cycle
const uint32_t EDGE_TIMEOUT_US  = 500000UL;  // 500ms

// Message accumulation timeout (ms)
const uint16_t MSG_TIMEOUT_MS   = 3000;

// Steven pulse classification
#define PULSE_BIT0   0
#define PULSE_BIT1   1
#define PULSE_START  2
#define PULSE_FIN    3
#define PULSE_TIMEOUT 4

// -------------------- Estado TX (PULSE) --------------------
char    msgBuffer[17];
int     msgLen  = 0;
uint8_t seqNum  = 0;
bool    txBusy  = false;

// -------------------- Buffer de tramas Steven recibidas --------------------
const uint8_t MAX_TRAMAS = 64;

struct TramaRx {
  char    caracter;
  uint8_t destino;
  uint8_t origen;
};

TramaRx  tramasRx[MAX_TRAMAS];
uint8_t  tramasLen          = 0;
uint32_t ultimaTramaMs      = 0;
bool     hayTramasPendientes = false;

// -------------------- Utilidad compartida --------------------
void waitUntil(uint32_t target) {
  while ((long)(micros() - target) < 0);
}

// ==================== LECTURA DEL SENSOR ====================

// Retorna true si hay luz (analog ≤ umbral)
bool readLight() {
  long sum = 0;
  for (int i = 0; i < 3; i++) {
    sum += analogRead(RX_ANALOG);
    delayMicroseconds(200);
  }
  return (sum / 3) <= LIGHT_THRESHOLD;
}

// ==================== TX: PULSE (Manchester) ====================

void sendHalf(bool level, uint32_t& t_next) {
  digitalWrite(TX_PIN, level ? HIGH : LOW);
  t_next += PULSE_HALF_US;
  waitUntil(t_next);
}

void sendManchesterBit(bool val, uint32_t& t_next) {
  sendHalf( val, t_next);
  sendHalf(!val, t_next);
}

void sendManchesterBits(uint8_t val, uint8_t n, uint32_t& t_next) {
  for (int i = n - 1; i >= 0; i--)
    sendManchesterBit(bitRead(val, i), t_next);
}

void sendFrame_PULSE(uint8_t id_orig, uint8_t id_dest, uint8_t seq,
                     const char* msg) {
  uint8_t len = strlen(msg);
  uint8_t payload[16];
  uint8_t chk = id_orig ^ id_dest ^ seq ^ len;

  for (int i = 0; i < len; i++) {
    payload[i] = msg[i] ^ PULSE_XOR_KEY;
    chk ^= payload[i];
  }

  digitalWrite(LED_PIN, HIGH);

  uint32_t t_next = micros();

  // Start pulse: HIGH 150ms, LOW 50ms
  digitalWrite(TX_PIN, HIGH);
  t_next += PULSE_START_HI;
  waitUntil(t_next);
  digitalWrite(TX_PIN, LOW);
  t_next += PULSE_START_LO;
  waitUntil(t_next);

  // Frame body
  sendManchesterBits(PULSE_PREAMBLE, 8, t_next);
  sendManchesterBits(id_orig,         4, t_next);
  sendManchesterBits(id_dest,         4, t_next);
  sendManchesterBits(seq,             4, t_next);
  sendManchesterBits(len,             8, t_next);
  for (int i = 0; i < len; i++)
    sendManchesterBits(payload[i], 8, t_next);
  sendManchesterBits(chk,             8, t_next);
  sendManchesterBits(PULSE_FOOTER,    8, t_next);

  // End guard
  digitalWrite(TX_PIN, LOW);
  t_next += PULSE_END_GUARD;
  waitUntil(t_next);

  digitalWrite(LED_PIN, LOW);
}

// ==================== RX: Steven (pulse-width) ====================

// Lee un pulso Steven midiendo la duracion en HIGH.
// Retorna PULSE_BIT0, PULSE_BIT1, PULSE_START, PULSE_FIN, o PULSE_TIMEOUT.
uint8_t readStevenPulse() {
  // Esperar oscuridad (por si venimos de un pulso anterior)
  uint32_t t0 = micros();
  while (readLight()) {
    if ((long)(micros() - t0) >= (long)EDGE_TIMEOUT_US) return PULSE_TIMEOUT;
  }

  // Esperar borde de subida
  while (!readLight()) {
    if ((long)(micros() - t0) >= (long)EDGE_TIMEOUT_US) return PULSE_TIMEOUT;
  }

  // Medir cuanto dura en HIGH
  uint32_t t_rise = micros();
  while (readLight()) {
    if ((long)(micros() - t_rise) >= 1000000L) break;  // 1s safety
  }
  uint32_t duration = micros() - t_rise;

  if (duration > STEVEN_THR_FIN)   return PULSE_FIN;    // >600ms → FIN (700)
  if (duration > STEVEN_THR_START) return PULSE_START;  // >400ms → START (500)
  if (duration > STEVEN_THR_BIT1)  return PULSE_BIT1;   // >200ms → bit 1 (300)
  return PULSE_BIT0;                                    // else  → bit 0 (100)
}

// Recibe una trama Steven completa y la acumula.
// Formato: START [4b dest][4b orig][5b len][5b data][5b chk] FIN
void receiveFrame_Steven() {
  // Esperar START
  uint8_t pulse = readStevenPulse();
  if (pulse != PULSE_START) return;

  // Leer 23 bits del cuerpo (4+4+5+5+5)
  uint32_t raw = 0;
  for (int i = 0; i < 23; i++) {
    pulse = readStevenPulse();
    if (pulse == PULSE_TIMEOUT) return;
    if (pulse != PULSE_BIT0 && pulse != PULSE_BIT1) return;
    raw = (raw << 1) | (pulse & 1);
  }

  // Consumir FIN (debe venir, pero ya tenemos los datos)
  pulse = readStevenPulse();
  // Si no es FIN, igual procesamos lo que tenemos

  // Extraer campos (MSB first)
  uint8_t dest = (raw >> 19) & 0x0F;
  uint8_t orig = (raw >> 15) & 0x0F;
  uint8_t len  = (raw >> 10) & 0x1F;
  uint8_t data = (raw >>  5) & 0x1F;
  uint8_t chk  = (raw >>  0) & 0x1F;

  // Validar
  if (len == 0 || len > 16) return;
  if (chk != data) {  // Steven: checksum == data cifrada
    Serial.print(F("[RX:Stev] Chk fail. Calc:"));
    Serial.print(data, HEX);
    Serial.print(F(" Rx:"));
    Serial.println(chk, HEX);
    return;
  }

  // Descifrar: 5-bit → ASCII (a=0..z=25)
  uint8_t decrypted = data ^ STEVEN_XOR_KEY;
  char c = 'a' + (decrypted & 0x1F);

  // Acumular
  if (tramasLen < MAX_TRAMAS) {
    tramasRx[tramasLen].caracter = c;
    tramasRx[tramasLen].destino  = dest;
    tramasRx[tramasLen].origen   = orig;
    tramasLen++;
  }

  ultimaTramaMs       = millis();
  hayTramasPendientes = true;

  Serial.print(F("[RX:Stev] dest:0x")); Serial.print(dest, HEX);
  Serial.print(F(" orig:0x"));          Serial.print(orig, HEX);
  Serial.print(F(" '"));
  if (c >= 'a' && c <= 'z') Serial.print(c); else Serial.print('?');
  Serial.print(F("' ("));
  Serial.print(tramasLen);
  Serial.println(F(" tramas acum.)"));
}

// Procesa el mensaje completo acumulado
void procesarMensajeCompleto() {
  if (tramasLen == 0) {
    hayTramasPendientes = false;
    return;
  }

  uint8_t destino = tramasRx[0].destino;
  uint8_t origen  = tramasRx[0].origen;

  Serial.println(F("=============================="));
  Serial.print(F("[MSG] ")); Serial.print(tramasLen);
  Serial.print(F(" tramas. Dest:0x")); Serial.print(destino, HEX);
  Serial.print(F(" Orig:0x")); Serial.println(origen, HEX);

  if (destino == MY_ID || destino == 0x0F) {
    // Es para nosotros
    Serial.print(F("[MSG:PARA_MI] \""));
    for (uint8_t i = 0; i < tramasLen; i++) {
      char cc = tramasRx[i].caracter;
      if (cc >= 'a' && cc <= 'z') Serial.print(cc);
    }
    Serial.println(F("\""));
  } else {
    // No es para nosotros → reenviar en PULSE
    Serial.println(F("[MSG:RELAY] Reenviando en PULSE..."));

    char relayBuf[17];
    uint8_t n = tramasLen;
    if (n > 16) n = 16;
    for (uint8_t i = 0; i < n; i++)
      relayBuf[i] = tramasRx[i].caracter;
    relayBuf[n] = '\0';

    txBusy = true;
    sendFrame_PULSE(origen, destino, seqNum++ & 0x0F, relayBuf);
    txBusy = false;

    Serial.print(F("[TX:RELAY] ")); Serial.print(n);
    Serial.println(F(" chars en trama PULSE."));
  }
  Serial.println(F("=============================="));

  tramasLen            = 0;
  hayTramasPendientes  = false;
}

// ==================== Manejo de entrada Serial ====================

void handleSerialInput() {
  if (!Serial.available()) return;

  char c = Serial.read();
  if (c == '\n' || c == '\r') {
    if (msgLen > 0) {
      msgBuffer[msgLen] = '\0';

      uint8_t dest;
      if (msgBuffer[0] == '@' && msgLen >= 2) {
        dest = msgBuffer[1] - '0';
        if (dest > 9) dest = msgBuffer[1] - 'A' + 10;
        if (dest > 15) dest = 0x02;
        for (int i = 0; i <= msgLen - 3; i++)
          msgBuffer[i] = msgBuffer[i + 2];
        msgLen -= 2;
        msgBuffer[msgLen] = '\0';
      } else {
        dest = 0x02;
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
  Serial.println(F(" TRANSCEPTOR — PULSE(TX) + Steven(RX)"));
  Serial.print  (F(" Mi ID: 0x")); Serial.println(MY_ID, HEX);
  Serial.println(F(" TX: PULSE (Manchester, 50ms/bit, XOR 0x42)"));
  Serial.println(F(" RX: Steven (pulse-width, XOR 0x5A, umbral=75)"));
  Serial.println(F(" @X msg → destino X. Escuchando..."));
  Serial.println(F("======================================"));
}

void loop() {
  // 1. Procesar entrada serial (no bloqueante)
  handleSerialInput();

  // 2. Verificar timeout: procesar mensaje completo acumulado
  if (hayTramasPendientes && (millis() - ultimaTramaMs > MSG_TIMEOUT_MS)) {
    procesarMensajeCompleto();
  }

  // 3. Si no estamos transmitiendo, intentar recibir trama Steven
  if (!txBusy) {
    receiveFrame_Steven();
  }
}
