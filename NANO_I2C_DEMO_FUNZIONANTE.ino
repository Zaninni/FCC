#include <Wire.h>

/*  Slave I2C: Arduino Nano
    - Riceve dal master l’indice del LED da accendere (0..LED_COUNT-1).
    - 255 = comando spegni-tutti.
    - LED con catodo comune (HIGH = acceso, LOW = spento).
    - Pulsante di reset su pin 4 (attivo-basso, con pull-up interno):
      alla pressione, lo slave pubblica 1 su richiesta I2C, poi torna 0
      dopo che il master ha letto (evento consumato).
*/

#define I2C_ADDRESS   0x08
#define RESET_BUTTON  4
#define ALL_OFF_CMD   255

enum LedIds {
  LED_STATE_ON,
  LED_STATE_OFF,
  LED_FORZATO,
  LED_OUT_HEATER,
  LED_OUT_PUMP,
  LED_OUT_CHILLER,
  LED_EMERGENCY_ACTIVE,
  LED_ALARM_MINP,
  LED_ALARM_MAXP,
  LED_ALARM_NOFLOW,
  LED_ALARM_CHFAULT,
  LED_ALARM_TMAX,
  LED_COUNT
};

// Mappatura pin LED (Nano): HIGH = acceso
const uint8_t ledPins[LED_COUNT] = { 5, 6, 7, 8, 9, 10, 11, 12, A0, A1, A2, A3 };

// Flag evento reset (letto da master)
volatile uint8_t resetRequest = 0;

// Debounce
const unsigned long DEBOUNCE_MS = 30;
unsigned long lastDebounceMs = 0;
bool rawPrev = HIGH;           // con INPUT_PULLUP: riposo = HIGH
bool stablePrev = HIGH;

void setup() {
  // LED
  for (uint8_t i = 0; i < LED_COUNT; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);   // spenti
  }

  // Pulsante: attivo-basso, pull-up interno
  pinMode(RESET_BUTTON, INPUT_PULLUP);

  // Debug
  Serial.begin(115200);

  // I2C Slave
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);   // comandi dal master
  Wire.onRequest(requestEvent);   // master chiede stato reset
}

void loop() {
  // Debounce pulsante e rilevazione fronte HIGH->LOW
  bool raw = digitalRead(RESET_BUTTON);
  if (raw != rawPrev) {
    rawPrev = raw;
    lastDebounceMs = millis();
  }

  if (millis() - lastDebounceMs >= DEBOUNCE_MS) {
    bool stable = rawPrev;
    if (stablePrev == HIGH && stable == LOW) {   // fronte di pressione
      resetRequest = 1;                          // pubblica evento
      Serial.println("Pulsante RESET premuto");
    }
    stablePrev = stable;
  }

  // altro codice non bloccante...
}

// ===== Slave RX: comandi dal master =====
void receiveEvent(int howMany) {
  while (Wire.available()) {
    int cmd = Wire.read();   // 0..255

    if (cmd == ALL_OFF_CMD) {
      // spegni tutti i LED
      for (uint8_t i = 0; i < LED_COUNT; i++) {
        digitalWrite(ledPins[i], LOW);
      }
    } else if (cmd >= 0 && cmd < LED_COUNT) {
      // accendi SOLO il LED richiesto
      //for (uint8_t i = 0; i < LED_COUNT; i++) {
      //  digitalWrite(ledPins[i], LOW);
      //}
      digitalWrite(ledPins[cmd], HIGH);
    } else {
      // comando fuori range: ignora
    }
  }
}

// ===== Slave TX: risposta al master =====
void requestEvent() {
  uint8_t resp = resetRequest;  // copia atomica
  Wire.write(resp);             // 0 o 1
  resetRequest = 0;             // evento consumato dopo la lettura del master
}
