#include <Wire.h>

// ==================== CONFIG ====================
const bool DEBUG_ENABLED = true;
const uint8_t I2C_SLAVE_ADDR = 0x12;

struct Timings {
  uint16_t sampleMs   = 20;
  uint16_t debounceMs = 30;
  uint16_t hbMs       = 500;
  uint16_t blinkMs    = 500;
} T;

const uint16_t COMM_PULSE_MS = 20;

// ====== PIN ======
const uint8_t PIN_LEVER_START_ON   = 2;
const uint8_t PIN_LEVER_FORCED     = 3;
const uint8_t PIN_RESET_BUTTON     = 4;

const uint8_t PIN_LED_HEARTBEAT    = LED_BUILTIN; // D13

// 12 LED HMI su D5..D12 + A0..A3
const uint8_t LED_PINS[12] = { 5,6,7,8,9,10,11,12, A0,A1,A2,A3 };

enum LedIndex : uint8_t {
  LED_STATE_ON = 0,
  LED_STATE_OFF,
  LED_OUT_HEATER,
  LED_OUT_PUMP,
  LED_OUT_CHILLER,
  LED_ALARM_MINP,
  LED_ALARM_MAXP,
  LED_ALARM_NOFLOW,
  LED_ALARM_CHFAULT,
  LED_ALARM_TEMP,
  LED_EMERGENCY,
  LED_FORZATO
};

struct Debouncer { bool stable=false, last=false; uint32_t t=0; } dbStart, dbForced, dbReset;

struct __attribute__((packed)) HmiToOpta {
  uint8_t  hmiBits;  // bit0=START, bit1=FORZ, bit2=RESET
  uint32_t monotonicMs;
} tx;

struct __attribute__((packed)) LedFrame {
  uint16_t on_bits;
  uint16_t blink_bits;
} rx;

uint16_t g_on_bits=0, g_blink_bits=0;
uint8_t  g_blink_phase=0;
uint32_t tBlink=0;
uint32_t tCommPulse=0;

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

void setup(){
  if (DEBUG_ENABLED){ Serial.begin(115200); delay(30); Serial.println(F("[UNO] HMI 12 LED")); }

  pinMode(PIN_LEVER_START_ON, INPUT);
  digitalWrite(PIN_LEVER_START_ON, LOW);
  pinMode(PIN_LEVER_FORCED  , INPUT);
  digitalWrite(PIN_LEVER_FORCED  , LOW);
  pinMode(PIN_RESET_BUTTON  , INPUT);
  digitalWrite(PIN_RESET_BUTTON  , LOW);

  for (uint8_t i=0;i<12;i++){ pinMode(LED_PINS[i], OUTPUT); digitalWrite(LED_PINS[i], LOW); }
  pinMode(PIN_LED_HEARTBEAT, OUTPUT); digitalWrite(PIN_LED_HEARTBEAT, LOW);

  Wire.begin(I2C_SLAVE_ADDR);
  Wire.onRequest(onI2CRequest);
  Wire.onReceive(onI2CReceive);
  // disable internal pull-ups on I2C lines
  pinMode(SDA, INPUT);
  pinMode(SCL, INPUT);
  digitalWrite(SDA, LOW);
  digitalWrite(SCL, LOW);
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
      Serial.print(F("[UNO] START="));  Serial.print(dbStart.stable);
      Serial.print(F(" FORZ="));        Serial.print(dbForced.stable);
      Serial.print(F(" RESET="));       Serial.println(dbReset.stable);
    }
  }
}
