// === DEBUG SENSOR: Lectura del fotodetector + control manual del laser ===
// Usa el boton en D2 (INPUT_PULLUP) para encender el laser en D8.
// Muestra en Serial las lecturas del sensor para identificar
// el umbral entre luz ambiente (dia) y laser directo.
//
// Pines:
//   D2  = Boton (a GND, usa pull-up interno)
//   D7  = Fotodetector / modulo LDR (DO — salida digital)
//   D8  = Laser
//   D10 = Sensor pin S (senal cruda del LDR)
//
// Serial: 115200 baud — lecturas rapidas sin saturar el monitor.

const int RX_PIN    = 7;
const int TX_PIN    = 8;
const int BTN_PIN   = 2;
const int SENSOR_S  = 10;   // pin S del modulo LDR (senal cruda)

void setup() {
  pinMode(RX_PIN, INPUT);
  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, LOW);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(SENSOR_S, INPUT);

  Serial.begin(115200);
  Serial.println(F("=== DEBUG SENSOR ==="));
  Serial.println(F("D7=DO(D), D10=S(analog), D8=laser, D2=boton"));
  Serial.println(F("Presiona el boton para encender el laser."));
  Serial.println(F("Observa el cambio en los valores para hallar el umbral.\n"));
  Serial.println(F("S_ana\tfiltD7\tlaserON"));
  Serial.println(F("-----\t------\t-------"));
}

void loop() {
  // Leer boton (LOW = presionado, por INPUT_PULLUP)
  bool laserON = (digitalRead(BTN_PIN) == LOW);
  digitalWrite(TX_PIN, laserON ? HIGH : LOW);

  // Leer pin S crudo del LDR (analogico)
  int sRaw = analogRead(SENSOR_S);

  // Muestrear el sensor digital (DO): 20 lecturas rapidas para filtro por mayoria
  int highCount = 0;
  for (int i = 0; i < 20; i++) {
    if (digitalRead(RX_PIN) == HIGH) highCount++;
  }

  // S_ana: lectura analogica cruda (0-1023)
  // filtD7: cuantas de 20 muestras del DO fueron HIGH
  Serial.print(sRaw);
  Serial.print(F("\t"));
  Serial.print(highCount);
  Serial.print(F("\t"));
  Serial.println(laserON ? F("SI") : F("NO"));

  delay(100);  // ~10 lecturas/seg, suficiente para ver cambios
}
