#include "Led.h"
#include <Wire.h>

// Aggiorna pin di uscita in base agli ingressi
void aggiornaLed() {
  if (ingressi[Ptemp].stato)   sendLedCommand(LED_ALARM_TMAX);
  if (ingressi[Pmin].stato)    sendLedCommand(LED_ALARM_MINP);
  if (ingressi[Pmax].stato)    sendLedCommand(LED_ALARM_MAXP);
  if (ingressi[Flow].stato)    sendLedCommand(LED_ALARM_NOFLOW);
  if (ingressi[Chiller].stato) sendLedCommand(LED_ALARM_CHFAULT);
  if (ingressi[Chiller].stato || ingressi[Chiller].stato || ingressi[Chiller].stato || ingressi[Chiller].stato || ingressi[Chiller].stato) sendLedCommand(LED_EMERGENCY_ACTIVE);
}

// Funzione per inviare comando LED allo slave
void sendLedCommand(byte cmd) {
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(cmd);
  Wire.endTransmission();
}