/*  Master I2C: Arduino OPTA
    - Premendo il pulsante integrato (USER_BTN) si accende
      il led successivo sullo slave.
    - Se si tenta di superare l’ultimo LED, il LED integrato
      del master lampeggia 3 volte.
    - Se lo slave richiede un reset (pulsante su Nano),
      il master spegne i LED e ricomincia.
*/

#include <Wire.h>
#include "Ingressi.h"


#define SLAVE_ADDR 0x08       // Indirizzo I2C dello slave

struct TempScale {
  float gain = 100.0f/4095.0f;
  float off  = -30.0f;
  float Tmax = 30.0f;
  float hyst = 1.0f;
} TS;

// PIN
const uint8_t AIN_TEMP010  = A4; //sonda temperatura
const uint8_t DOUT_HEATER  = D0;
const uint8_t DOUT_PUMP    = D1;
const uint8_t DOUT_CHILLER = D2;
// funzione temperatura
static inline float readTempC(){ uint16_t a=analogRead(AIN_TEMP010); return TS.gain*a+TS.off; }
// LETTURA STATO



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
  setupIngressi();
  
  pinMode(LED_OUT_PUMP,OUTPUT);
  pinMode(DOUT_PUMP,OUTPUT);
  pinMode(LED_OUT_CHILLER,OUTPUT);
  pinMode(DOUT_CHILLER,OUTPUT);
  pinMode(LED_OUT_HEATER,OUTPUT);
  pinMode(DOUT_HEATER,OUTPUT);
   }

void loop() {
  sendLedCommand(255); //Comando refresh dei led
  float tC = readTempC();    // legge la temperatura
  aggiornaIngressi(tC);      // aggiorna tutti gli ingressi
  // Esempio di utilizzo
  if (ingressi[POFF].stato) Serial.println("POFF attivo!");
  if (ingressi[ONOFF].stato) Serial.println("ONOFF attivo!");
  if (ingressi[POVER].stato) Serial.println("POVER attivo!");
  if (ingressi[RESET].stato) Serial.println("RESET attivo!");
  if (ingressi[Ptemp].stato) Serial.println("Temperatura alta!");



  
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
