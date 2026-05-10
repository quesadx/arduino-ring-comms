// === DEBUG SENSOR: Lectura del fotodetector + control manual del laser ===
// Usa el boton en D2 (INPUT_PULLUP) para encender el laser en D8.
// Muestra en Serial las lecturas del sensor en D7 para identificar
// el umbral entre luz ambiente (dia) y laser directo.
//
// Pines:
//   D2  = Boton (a GND, usa pull-up interno)
//   D7  = Fotodetector / modulo LDR (DO)
//   D8  = Laser
//
// Serial: 115200 baud — lecturas rapidas sin saturar el monitor.

const int RX_PIN    = 7;
const int TX_PIN    = 8;
const int BTN_PIN   = 2;

void setup() {
  pinMode(RX_PIN, INPUT);
  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, LOW);
  pinMode(BTN_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.println(F("=== DEBUG SENSOR ==="));
  Serial.println(F("Pin D7 = sensor, D8 = laser, D2 = boton"));
  Serial.println(F("Presiona el boton para encender el laser."));
  Serial.println(F("Observa el cambio en los valores para hallar el umbral.\n"));
  Serial.println(F("highCount\trawAvg\tlaserON"));
  Serial.println(F("---------\t------\t-------"));
}

void loop() {
  // Leer boton (LOW = presionado, por INPUT_PULLUP)
  bool laserON = (digitalRead(BTN_PIN) == LOW);
  digitalWrite(TX_PIN, laserON ? HIGH : LOW);

  // Muestrear el sensor: 20 lecturas rapidas para filtro por mayoria
  int highCount = 0;
  int rawSum = 0;
  for (int i = 0; i < 20; i++) {
    int raw = digitalRead(RX_PIN);
    rawSum += raw;
    if (raw == HIGH) highCount++;
  }

  // rawAvg: promedio de las 20 lecturas (0.0 - 1.0 aprox)
  // highCount: cuantas de 20 fueron HIGH (>10 = filtro lo toma como HIGH)
  Serial.print(highCount);
  Serial.print(F("\t\t"));
  Serial.print(rawSum / 20.0, 2);
  Serial.print(F("\t"));
  Serial.println(laserON ? F("SI") : F("NO"));

  delay(100);  // ~10 lecturas/seg, suficiente para ver cambios
}
