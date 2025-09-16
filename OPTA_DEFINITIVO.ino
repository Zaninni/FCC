/*  Master I2C: Arduino OPTA
    - Premendo il pulsante integrato (USER_BTN) si accende
      il led successivo sullo slave.
    - Se si tenta di superare l’ultimo LED, il LED integrato
      del master lampeggia 3 volte.
    - Se lo slave richiede un reset (pulsante su Nano),
      il master spegne i LED e ricomincia.
*/

#include <Wire.h>

#define SLAVE_ADDR 0x08       // Indirizzo I2C dello slave

struct TempScale {
  float gain = 100.0f/4095.0f;
  float off  = -30.0f;
  float Tmax = 30.0f;
  float hyst = 1.0f;
} TS;

// PIN
const uint8_t AIN_TEMP010  = A4; //sonda temperatura
const uint8_t DIN_MINP     = A0;
const uint8_t DIN_MAXP     = A1;
const uint8_t DIN_FLOW_OK  = A2;
const uint8_t DIN_CHFAULT  = A3;
const uint8_t DIN_POWEROFF = A7;
const uint8_t DOUT_HEATER  = D0;
const uint8_t DOUT_PUMP    = D1;
const uint8_t DOUT_CHILLER = D2;

// funzione temperatura
static inline float readTempC(){ uint16_t a=analogRead(AIN_TEMP010); return TS.gain*a+TS.off; }
// LETTURA STATO




// Sostituire con il pin effettivo del pulsante integrato di OPTA
//#define BTN_USER 4
//#define LED_BUILTIN

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

#define ALL_OFF_CMD 255
#define REFRESH_ALL_LED 254
const unsigned long DEBOUNCE_MS = 30;

int currentLed = -1;               // -1 = nessun LED selezionato ancora
bool rawPrev = HIGH;               // con INPUT_PULLUP: riposo=HIGH
bool stablePrev = HIGH;
unsigned long lastChangeMs = 0;

void setup() {
  analogReadResolution(12);
  Wire.begin();              // Master
  pinMode(BTN_USER, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DIN_MAXP, INPUT);
  pinMode(DIN_MINP, INPUT);
  pinMode(DIN_FLOW_OK, INPUT);
  pinMode(DIN_CHFAULT, INPUT);
  pinMode(DIN_POWEROFF, INPUT);
  pinMode(LED_OUT_PUMP,OUTPUT);
  pinMode(DOUT_PUMP,OUTPUT);
  pinMode(LED_OUT_CHILLER,OUTPUT);
  pinMode(DOUT_CHILLER,OUTPUT);
  pinMode(LED_OUT_HEATER,OUTPUT);
  pinMode(DOUT_HEATER,OUTPUT);
   }

void loop() {
  // 1. Verifica se lo slave richiede un reset
  Wire.requestFrom(SLAVE_ADDR, 1);
  if (Wire.available()) {
    byte resetFlag = Wire.read();
    if (resetFlag == 1) {
      currentLed = -1;
      sendLedCommand(255);  // Comando di reset: spegni tutti i LED sullo slave
    }
  }

sendLedCommand(255); //Comando refresh dei led

/* 2. Gestione del pulsante integrato su OPTA (active-low)
bool raw = digitalRead(BTN_USER);
if (raw != rawPrev) {              // variazione grezza -> (ri)avvia debounce
  rawPrev = raw;
  lastChangeMs = millis();
}

if (millis() - lastChangeMs >= DEBOUNCE_MS) {
  bool stable = raw;               // stato debounced

  // fronte di pressione: HIGH -> LOW (pulsante premuto)
  if (stablePrev == HIGH && stable == LOW) {
    Serial.println("Pulsante master premuto");

    // prova a passare al LED successivo
    int next = currentLed + 1;

    if (next < LED_COUNT) {
      currentLed = next;
      sendLedCommand((uint8_t)currentLed);   // invia indice 0..LED_COUNT-1
    } else {
      // oltre l’ultimo: clamp e lampeggia per segnalare “fine lista”
      currentLed = LED_COUNT - 1;

      for (int i = 0; i < 3; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(100);
        digitalWrite(LED_BUILTIN, LOW);
        delay(100);
      }
      // opzionale: spegni tutto allo slave se vuoi “reset visivo”
      // sendLedCommand(ALL_OFF_CMD);
      // currentLed = -1;
    }
  }
  stablePrev = stable;
}
 INE GESTIONE PULSANTE  
*/


  float tC = readTempC();
  Serial.print("La temperatura è: ");
  Serial.println(tC);
  Serial.println(analogRead(AIN_TEMP010));
  bool aTemp = tC>=TS.Tmax;
  bool aPmin = digitalRead(DIN_MINP);
  bool aPmax = digitalRead(DIN_MAXP);
  bool aFlow = digitalRead(DIN_FLOW_OK);
  bool aChil = digitalRead(DIN_CHFAULT);
  bool poff = digitalRead(DIN_POWEROFF);
  
  if (aTemp){
    Serial.println("Allarme temperatura");
    sendLedCommand(LED_ALARM_TMAX);
    }

  if (!aPmin){
    Serial.println("Allarme pressione minima");
    sendLedCommand(LED_ALARM_MINP);
    }
  if (!aPmax){
    Serial.println("Allarme pressione massima");
    sendLedCommand(LED_ALARM_MAXP);
    }
  if (!aFlow){
    Serial.println("Allarme flussostato");
    sendLedCommand(LED_ALARM_NOFLOW);
  }
  if (!aChil){
    Serial.println("Allarme chiller");
    sendLedCommand(LED_ALARM_CHFAULT);
  }



  //Allarme Master
  if (aTemp || !aPmax || !aPmin || !aFlow || !aChil){
    Serial.println("Allarme generale");
    sendLedCommand(LED_EMERGENCY_ACTIVE);
  }
  if (poff){ 
    //STACCO POMPA HEATER CHILLER
    digitalwrite(DOUT_PUMP,LOW);
    digitalwrite(DOUT_HEATER,LOW);
    digitalwrite(DOUT_CHILLER,LOW);
    //sendLedCommand(LED_OUT_PUMP);  //QUI VANNO SPENTI I LED
    //sendLedCommand(LED_OUT_HEATER);
    //sendLedCommand(LED_OUT_CHILLER);
  }


}


// Invia al Nano l’indice del LED da accendere o 0xFF per spegnere tutto
void sendLedCommand(byte cmd) {
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(cmd);
  Wire.endTransmission();
}
