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
#include "Led.h"


#define SLAVE_ADDR 0x08       // Indirizzo I2C dello slave



// PIN

const uint8_t DOUT_HEATER  = D0;
const uint8_t DOUT_PUMP    = D1;
const uint8_t DOUT_CHILLER = D2;
// funzione temperatura

// LETTURA STATO





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

  aggiornaIngressi();      // aggiorna tutti gli ingressi
  aggiornaLed();          // aggiorna pin di uscita
  sendLedCommand(255);

  // Esempio di utilizzo
  if (ingressi[POFF].stato) Serial.println("POFF attivo!");
  if (ingressi[ONOFF].stato) Serial.println("ONOFF attivo!");
  if (ingressi[POWER].stato) Serial.println("POWER attivo!");
  if (ingressi[RESET].stato) Serial.println("RESET attivo!");
  if (ingressi[Ptemp].stato) Serial.println("Temperatura alta!");



  






}


