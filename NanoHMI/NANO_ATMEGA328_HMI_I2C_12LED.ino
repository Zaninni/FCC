/*
  NANO_ATMEGA328_HMI_I2C_12LED
  - HMI per OPTA via I2C (SDA=A4, SCL=A5), SLAVE @0x12
  - Implementazione per Arduino NANO basato su ATmega328P
  - Leve/pulsante con pull-down esterni (HIGH = attivo)
  - 2 leve: START_ON, FORCED_PUMP
  - 1 pulsante: RESET
  - 12 LED HMI (ON/OFF + allarmi) con opzionale lampeggio
  - LED integrato (D13) come heartbeat e activity I2C
  - Tutti gli ingressi usano `pinMode(..., INPUT)` per disabilitare i pull-up interni
*/

#include <Wire.h>

// ==================== CONFIG ====================
const bool DEBUG_ENABLED = true;      // unico interruttore debug
const uint8_t I2C_SLAVE_ADDR = 0x12;  // indirizzo I2C dello slave

struct Timings {
  uint16_t sampleMs   = 20;   // scansione ingressi
  uint16_t debounceMs = 30;   // antirimbalzo
  uint16_t hbMs       = 500;  // heartbeat LED integrato
  uint16_t blinkMs    = 500;  // periodo lampeggio LED
} T;

// Durata impulso sul LED integrato per indicare attività I2C (ms)
const uint16_t COMM_PULSE_MS = 20;

// ====== PIN ======
const uint8_t PIN_LEVER_START_ON   = 2;  // leva START/ON
const uint8_t PIN_LEVER_FORCED     = 3;  // leva FORZATO
const uint8_t PIN_RESET_BUTTON     = 4;  // pulsante RESET

const uint8_t PIN_LED_HEARTBEAT    = LED_BUILTIN; // D13

// 12 LED HMI (attivo-alto)
const uint8_t LED_PINS[12] = {
  5, 6, 7, 8, 9, 10, 11, 12, A0, A1, A2, A3
};

// Indici semantici dei 12 LED
enum LedIndex : uint8_t {
  LED_STATE_ON = 0,
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
  LED_ALARM_TMAX
};

// =================== STATO =======================
struct Debouncer { bool stable=false, last=false; uint32_t t=0; } dbStart, dbForced, dbReset;

// HMI -> OPTA: 3 bit (START, FORZATO, RESET) + timestamp
struct __attribute__((packed)) HmiToOpta {
  uint8_t  hmiBits;  // bit0=START, bit1=FORZ, bit2=RESET
  uint32_t monotonicMs;
} tx;

// OPTA -> HMI: 12 bit ON + 12 bit BLINK
struct __attribute__((packed)) LedFrame {
  uint16_t on_bits;
  uint16_t blink_bits;
} rx;

uint16_t g_on_bits=0, g_blink_bits=0;
uint8_t  g_blink_phase=0;
uint32_t tBlink=0;

// timestamp dell'ultima ricezione I2C
uint32_t tCommPulse=0;

// ================== UTILITY ======================
static inline bool debounceRead(Debouncer& d, uint8_t pin, uint16_t win){
  bool v = digitalRead(pin);
  if (v != d.last){ if (millis()-d.t >= win){ d.last=v; d.t=millis(); } }
  else d.t=millis();
  d.stable = d.last; return d.stable;
}

void applyLedFrame(){
  if (millis()-tBlink >= T.blinkMs){ tBlink=millis(); g_blink_phase ^= 1; }
  for (uint8_t i=0;i<12;i++){
    bool on  = (g_on_bits    >> i) & 0x1;
    bool bl  = (g_blink_bits >> i) & 0x1;
    bool val = on || (bl && g_blink_phase);
    digitalWrite(LED_PINS[i], val ? HIGH : LOW);
  }
}

// ================== I2C ==========================
void onI2CRequest(){
  uint8_t bits = 0;
  if (dbStart.stable)  bits |= (1u<<0);
  if (dbForced.stable) bits |= (1u<<1);
  if (dbReset.stable)  bits |= (1u<<2);
  tx.hmiBits = bits;
  tx.monotonicMs = millis();
  Wire.write((uint8_t*)&tx, sizeof(tx));
}

void onI2CReceive(int n){
  if (n == sizeof(LedFrame)){
    Wire.readBytes((char*)&rx, sizeof(rx));
    g_on_bits    = rx.on_bits    & 0x0FFF;
    g_blink_bits = rx.blink_bits & 0x0FFF;
    tCommPulse = millis();
  } else {
    while (Wire.available()) (void)Wire.read();
  }
}

// ================== SETUP/LOOP ===================
void setup(){
  if (DEBUG_ENABLED){ Serial.begin(115200); delay(30); Serial.println(F("[NANO] HMI 12 LED")); }

  pinMode(PIN_LEVER_START_ON, INPUT);
  pinMode(PIN_LEVER_FORCED  , INPUT);
  pinMode(PIN_RESET_BUTTON  , INPUT);

  for (uint8_t i=0;i<12;i++){ pinMode(LED_PINS[i], OUTPUT); digitalWrite(LED_PINS[i], LOW); }
  pinMode(PIN_LED_HEARTBEAT, OUTPUT); digitalWrite(PIN_LED_HEARTBEAT, LOW);

  Wire.begin(I2C_SLAVE_ADDR);
  Wire.onRequest(onI2CRequest);
  Wire.onReceive(onI2CReceive);
}

void loop(){
  static uint32_t tS=0, tHB=0;

  bool commActive = (millis() - tCommPulse) < COMM_PULSE_MS;
  if (commActive) {
    digitalWrite(PIN_LED_HEARTBEAT, HIGH);
  } else if (millis()-tHB >= T.hbMs) {
    tHB = millis();
    digitalWrite(PIN_LED_HEARTBEAT, !digitalRead(PIN_LED_HEARTBEAT));
  }

  applyLedFrame();

  if (millis()-tS >= T.sampleMs){
    tS = millis();
    debounceRead(dbStart , PIN_LEVER_START_ON , T.debounceMs);
    debounceRead(dbForced, PIN_LEVER_FORCED   , T.debounceMs);
    debounceRead(dbReset , PIN_RESET_BUTTON   , T.debounceMs);

    if (DEBUG_ENABLED){
      Serial.print(F("[NANO] START="));  Serial.print(dbStart.stable);
      Serial.print(F(" FORZ="));        Serial.print(dbForced.stable);
      Serial.print(F(" RESET="));       Serial.println(dbReset.stable);
    }
  }
}

