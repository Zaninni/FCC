#include "Ingressi.h"
#include <Wire.h>

// Definizione ingressi
// La seconda colonna gestisce la logica
Ingresso ingressi[NUM_INGRESSI] = {
  {"TempAlta", -1, false, false},        // virtuale
  {"Pmin", DIN_MINP, false, true},       // LOW = attivo
  {"Pmax", DIN_MAXP, false, true},       // LOW = attivo
  {"Flow", DIN_FLOW_OK, false, true},    // LOW = attivo
  {"Chiller", DIN_CHFAULT, false, true}, // LOW = attivo
  {"POFF", DIN_POFF, false, true},       // LOW = attivo
  {"ONOFF", -1, false, false},           // byte I2C
  {"POVER", -1, false, false},           // byte I2C
  {"RESET", -1, false, false}            // byte I2C
};

// Setup pin fisici
void setupIngressi() {
  for (int i = 0; i < NUM_INGRESSI; i++) {
    if (ingressi[i].pin != -1) {
      pinMode(ingressi[i].pin, INPUT_PULLUP);
    }
  }
}

// Aggiorna tutti gli ingressi (fisici, virtuali, I2C)
void aggiornaIngressi(float tC) {
  // 1️⃣ ingressi fisici
  for (int i = 0; i < NUM_INGRESSI; i++) {
    if (ingressi[i].pin != -1) { // pin fisico
      bool lettura = digitalRead(ingressi[i].pin);
      if (ingressi[i].trueSeLow) {
        ingressi[i].stato = !lettura; // LOW = attivo
      } else {
        ingressi[i].stato = lettura;  // HIGH = attivo
      }
    }
  }

  // 2️⃣ ingresso virtuale temperatura
  ingressi[Ptemp].stato = (tC >= TS.Tmax);

  // 3️⃣ ingressi dallo slave I2C: ONOFF, POVER, RESET
  const int SLAVE_ADDR = 8;
  Wire.requestFrom(SLAVE_ADDR, 3);
  byte buffer[3];
  int i = 0;
  while (Wire.available() && i < 3) {
    buffer[i++] = Wire.read();
  }

  if (i == 3) { // se ricevo tutti e 3 i byte
    ingressi[ONOFF].stato = buffer[0];
    ingressi[POWER].stato = buffer[1];
    ingressi[RESET].stato = buffer[2];
  }
}
