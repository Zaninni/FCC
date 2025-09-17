#include "Led.h"
#include <Wire.h>

// Aggiorna pin di uscita in base agli ingressi
void aggiornaLed() {
  //sendLedCommand(255);
  delay(2);
  if (ingressi[Chiller].stato) sendLedCommand(LED_ALARM_CHFAULT);
  delay(2);
  if (ingressi[Ptemp].stato)   sendLedCommand(LED_ALARM_TMAX);
  delay(2);
  if (ingressi[Pmin].stato)    sendLedCommand(LED_ALARM_MINP);
  delay(2);
  if (ingressi[Pmax].stato)    sendLedCommand(LED_ALARM_MAXP);
  delay(2);
  if (ingressi[Flow].stato)    sendLedCommand(LED_ALARM_NOFLOW);
  delay(2);
  if (ingressi[Ptemp].stato || ingressi[Pmin].stato || ingressi[Pmax].stato || ingressi[Flow].stato || ingressi[Chiller].stato) sendLedCommand(LED_EMERGENCY_ACTIVE);
  delay(2);

  if (ingressi[PUMP_OV].stato)  sendLedCommand(LED_FORZATO);
  delay(2);

  if (!ingressi[ONOFF].stato)  sendLedCommand(LED_STATE_ON);
  else if (ingressi[ONOFF].stato)  sendLedCommand(LED_STATE_OFF);
  delay(2);

  
  //sendLedCommand(LED_ALARM_TMAX);
}

// Funzione per inviare comando LED allo slave
void sendLedCommand(byte cmd) {
  Wire.beginTransmission(SLAVE_ADDR);
  Wire.write(cmd);
  Wire.endTransmission();
}
