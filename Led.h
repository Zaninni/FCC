#ifndef LED_H
#define LED_H

#include <Arduino.h>
#include "Pin_ingresso.h"
#include "Ingressi.h"

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


void aggiornaLed();
void sendLedCommand(byte);

#endif