#ifndef INGRESSI_H
#define INGRESSI_H

#include <Arduino.h>
#include "Pin_ingresso.h"  // include pin definiti

struct Ingresso {
  const char* nome;
  int pin;           // -1 se virtuale/I2C
  bool stato;        // stato corrente
  bool trueSeLow;    // true se lo stato logico è LOW = attivo
};

// Struttura per i parametri di conversione
struct TempScale {
  float gain;
  float off;
  float Tmax;
  float hyst;
};

// Enum per accesso simbolico agli ingressi (L'ORDINE DEVE ESSERE LO STESSO DI INGRESSI CPP)
enum IngressiIndex {
  Ptemp,   // ingresso virtuale temperatura alta
  Pmin,
  Pmax,
  Flow,
  Chiller,
  POFF,    // pin fisico
  ONOFF,   // byte I2C
  PUMP_OV,   // byte I2C
  RESETSW,   // byte I2C
  NUM_INGRESSI
};

// Array globale di ingressi
extern Ingresso ingressi[NUM_INGRESSI];

// Funzioni
void setupIngressi();
void aggiornaIngressi();

#endif