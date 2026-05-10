Receptor /* * RFC-UNA-2026-ULNET XA -- ARDUINO RECEPTOR * Lectura analogica con umbral robusto. * Decodifica 5 bits por carácter (a=00000...z=11001) * * Hardware: * Pin S del modulo KY-018 -> A0 del Arduino * Pin medio (VCC) -> 5V * Pin - -> GND *

pasted



Emisor /* * ============================================================ * RFC-UNA-2026-ULNET XA -- ARDUINO EMISOR * Protocolo ULNET para Comunicación en Anillo de Luz * Autores: Alex Josué Zúñiga Monge, Ximena María Cabezas Espinoza * =============================================

pasted



RFC-UNA-2026 Protocolo para Comunicacion en Anillo Universidad Nacional — Comunicacion y Redes de Computadores — I Ciclo 2026 Autores Fecha Steven Mata, Samuel Picado, Jeremy Navarrete 23/04/2026 — I Ciclo 2026 1. CAPA FISICA El protocolo define cuatro tipos de pulso, cada uno identi

pasted


Tengo estos 2 codigos para arduinos, el proyecto costa en hacer en hacer un anillo por ende ocupo que crees un codigo el cual analice cual llega, espere analice el la trama y vea si es para el y analice la trama y si no es para el envie la trama ademas de estos codigos que tengo ya sirve, pero están separados por emisor y receptor, ocupo que juntes el codigo ademas de que en la parte del emisor debe seguir la sfc de otra trama, deme tomar la trama que lleva con el formato de otro srf de otro y  tomar la trama, luego desempaquetar la trama y ver si es para nosotros y si es para nosotros tomarla y enseñarla y si no empaquetarla con nuestra trama y enbviarlar los codigos nuestros serian estos Los 2 primeros son los de nuestra trama, debes juntarlo para analizar, cape recalcar que en la parte de bist de origen y destino son de 4 bits y no 2 bits, ademas 

Contenido pegado
7.15 KB •191 líneas
•
El formato puede ser inconsistente con la fuente

Emisor
/*
 * ============================================================
 *   RFC-UNA-2026-ULNET XA  --  ARDUINO EMISOR
 *   Protocolo ULNET para Comunicación en Anillo de Luz
 *   Autores: Alex Josué Zúñiga Monge, Ximena María Cabezas Espinoza
 * ============================================================
 *
 * Stack de protocolos implementado:
 *   - Capa Física: presencia/ausencia de luz, 100 ms por bit
 *   - Capa Enlace: trama con direccionamiento + checksum XOR
 *   - Capa Seguridad: cifrado XOR con clave fija 0x5A
 *
 * Codificación: 5 bits por carácter (a=00000, b=00001, ... z=11001)
 *
 * Hardware:
 *   - LED (o láser) conectado al pin digital 8, ánodo al pin
 *     y cátodo a GND a través de una resistencia de 220 ohm.
 *
 * Uso:
 *   - Abrir el Monitor Serial a 9600 baudios.
 *   - Escribir un mensaje (solo a-z) y presionar Enter.
 *   - El Arduino codifica cada carácter a 5 bits, cifra, construye 
 *     la trama y la transmite parpadeando el LED bit a bit.
 */

// ----------- CONFIGURACIÓN DEL PROTOCOLO ULNET XA ------------
const uint8_t  PIN_TX        = 8;
const uint16_t BIT_DURATION  = 100;
const uint8_t  PREAMBLE      = 0xFE;   // 11111110
const uint8_t  XOR_KEY       = 0x5A;   // 01011010

const uint8_t MY_ADDR   = 0b01;
const uint8_t DEST_ADDR = 0b10;

// --- Buffer para recibir el mensaje completo por Serial ---
const uint8_t MAX_MSG = 64;
char msgBuffer[MAX_MSG];
uint8_t msgLen = 0;
// ----------------------------------------------------------------

void setup() {
  pinMode(PIN_TX, OUTPUT);
  digitalWrite(PIN_TX, LOW);
  Serial.begin(9600);
  Serial.println(F("-------------------------------------------"));
  Serial.println(F("  EMISOR Gabriel listo"));
  Serial.print  (F("  Nodo emisor  : ")); Serial.println(MY_ADDR,   BIN);
  Serial.print  (F("  Nodo destino ")); Serial.println(DEST_ADDR, BIN);
  Serial.println(F("  Escriba un mensaje (a-z) y presione ENTER"));
  Serial.println(F("----------------------------------------------"));
}

// ============================================================
//   LOOP: acumula caracteres hasta recibir ENTER, luego
//   envía cada carácter como una trama independiente.
// ============================================================
void loop() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      // ENTER recibido → procesar el mensaje acumulado
      if (msgLen > 0) {
        Serial.println(F("=============================================="));
        Serial.print  (F("Mensaje completo   : \""));
        for (uint8_t i = 0; i < msgLen; i++) Serial.print(msgBuffer[i]);
        Serial.println(F("\""));
        Serial.print  (F("Total de tramas    : ")); Serial.println(msgLen);
        Serial.println(F("=============================================="));

        // Enviar una trama por cada carácter del mensaje
        for (uint8_t i = 0; i < msgLen; i++) {
          Serial.print(F("--- Trama ")); Serial.print(i + 1);
          Serial.print(F(" de ")); Serial.print(msgLen);
          Serial.println(F(" ---"));
          enviarCaracter(msgBuffer[i]);
        }

        Serial.println(F("=============================================="));
        Serial.println(F("  Mensaje completo transmitido."));
        Serial.println(F("=============================================="));

        // Limpiar buffer para el próximo mensaje
        msgLen = 0;
      }
    } else {
      // Acumular carácter si hay espacio en el buffer
      if (msgLen < MAX_MSG) {
        msgBuffer[msgLen++] = c;
      } else {
        Serial.println(F("[AVISO] Buffer lleno, carácter descartado."));
      }
    }
  }
}

/* ============================================================
 *   CONVERSIÓN: carácter a código 5-bits (a=00000...z=11001)
 * ============================================================ */
uint8_t charA5bits(char c) {
  // Convertir a minúscula si es necesario
  if (c >= 'A' && c <= 'Z') {
    c = c - 'A' + 'a';
  }
  
  // Devolver valor de 5 bits (a=0, b=1, ..., z=25)
  if (c >= 'a' && c <= 'z') {
    return (uint8_t)(c - 'a');
  }
  return 0; // Por defecto 'a'
}

/* ============================================================
 *   ENVÍO DE UN CARÁCTER (5 bits según RFC-UNA-2026)
 * ============================================================ */
void enviarCaracter(char c) {
  // ---- Convertir carácter a 5 bits ----
  uint8_t dato5bits = charA5bits(c);
  
  // ---- CAPA DE SEGURIDAD: cifrado XOR ----
  uint8_t datosCifrados = dato5bits ^ XOR_KEY;

  Serial.println(F("----------------------------------------------"));
  Serial.print(F("Carácter original  : '")); Serial.print(c);
  Serial.print(F("'"));
  Serial.println();
  Serial.print(F("Código 5-bits      : ")); imprimirBin(dato5bits,     5); Serial.println();
  Serial.print(F("Clave XOR          : ")); imprimirBin(XOR_KEY,       8); Serial.println();
  Serial.print(F("Datos cifrados     : ")); imprimirBin(datosCifrados, 5); Serial.println();

  // Longitud siempre es 1 (un carácter por trama)
  uint8_t longitud = 1;
  
  // Calcular checksum XOR del dato cifrado
  uint8_t checksum = calcularChecksum(datosCifrados);

  Serial.println(F("Trama a transmitir :"));
  Serial.print(F("  Preambulo : ")); imprimirBin(PREAMBLE,      8); Serial.println();
  Serial.print(F("  Destino   : ")); imprimirBin(DEST_ADDR,     2); Serial.println();
  Serial.print(F("  Origen    : ")); imprimirBin(MY_ADDR,       2); Serial.println();
  Serial.print(F("  Longitud  : ")); imprimirBin(longitud,      5); Serial.println();
  Serial.print(F("  Datos cif : ")); imprimirBin(datosCifrados, 5); Serial.println();
  Serial.print(F("  Checksum  : ")); imprimirBin(checksum,      5); Serial.println();

  Serial.println(F("Transmitiendo por luz..."));

  enviarByte(PREAMBLE,      8);
  enviarByte(DEST_ADDR,     2);
  enviarByte(MY_ADDR,       2);
  enviarByte(longitud,      5);
  enviarByte(datosCifrados, 5);
  enviarByte(checksum,      5);

  digitalWrite(PIN_TX, LOW);
  delay(BIT_DURATION * 4);   // espacio entre tramas

  Serial.println(F("Trama enviada."));
}

/* ============================================================
 *   CAPA FÍSICA
 * ============================================================ */
void enviarBit(uint8_t bit) {
  digitalWrite(PIN_TX, bit ? HIGH : LOW);
  delay(BIT_DURATION);
}

void enviarByte(uint8_t valor, uint8_t nBits) {
  for (int i = nBits - 1; i >= 0; i--) {
    enviarBit((valor >> i) & 0x01);
  }
}

/* ============================================================
 *   CHECKSUM XOR (del campo DATOS)
 * ============================================================ */
uint8_t calcularChecksum(uint8_t datos) {
  // El checksum XOR es simplemente el dato mismo en este caso
  // (un solo dato), pero se calcula sobre 5 bits
  return datos;
}

/* ============================================================
 *   UTILIDAD
 * ============================================================ */
void imprimirBin(uint8_t valor, uint8_t nBits) {
  for (int i = nBits - 1; i >= 0; i--) {
    Serial.print((valor >> i) & 0x01);
  }
}