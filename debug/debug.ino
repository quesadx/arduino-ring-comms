// === DEBUG SENSOR: Nivel de luz crudo del LDR ===
// Muestra la lectura analogica del pin S del modulo LDR (pin 10).
// Boton en D2 enciende el laser en D8 para comparar luz ambiente vs laser.
//
// NOTA: El sensor tiene logica invertida —
//   mucha luz = valor bajo (~0),   poca luz = valor alto (~1023).
//   El laser deberia hacer caer el valor al iluminar el sensor.
//
// Pines:
//   D2  = Boton (a GND, pull-up interno)
//   D8  = Laser
//   D10 = Pin S del LDR (senal analogica de nivel de luz)
//
// Serial: 9600 baud (changed for compatibility with other sketches)

const int TX_PIN    = 8;
const int BTN_PIN   = 2;
const int SENSOR_S  = A0; // use analog pin A0 for light sensor

void setup() {
  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, LOW);
  pinMode(BTN_PIN, INPUT_PULLUP);

  Serial.begin(9600);
  Serial.println(F("=== DEBUG SENSOR ==="));
  Serial.println(F("Lectura analogica del pin S (D10) del LDR."));
  Serial.println(F("Boton D2 -> enciende laser D8."));
  Serial.println(F("Sensor invertido: luz=bajo, oscuridad=alto.\n"));
  Serial.println(F("luz\tlaser"));
  Serial.println(F("---\t-----"));
}

void loop() {
  bool laserON = (digitalRead(BTN_PIN) == LOW);
  digitalWrite(TX_PIN, laserON ? HIGH : LOW);

  // Promedio de 8 lecturas para suavizar ruido
  long sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(SENSOR_S);
    delayMicroseconds(200);
  }
  int luz = sum / 8;

  // Print clear numeric value and laser state on separate lines
  Serial.print(F("luz="));
  Serial.println(luz);
  Serial.print(F("laser="));
  Serial.println(laserON ? F("SI") : F("NO"));
  Serial.println(); // blank line to separate samples for easy reading

  delay(50);
}
