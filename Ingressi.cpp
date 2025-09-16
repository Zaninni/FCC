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
  {"POFF", DIN_POWEROFF, false, true},       // LOW = attivo
  {"ONOFF", -1, false, false},           // byte I2C
  {"POVER", -1, false, false},           // byte I2C
  {"RESETSW", -1, false, false}            // byte I2C
};


// Inizializzazione dei valori
TempScale TS = {
  100.0f / 4095.0f,  // gain
  -30.0f,            // off
  30.0f,             // Tmax
  1.0f               // hyst
};

// Funzione privata al file e legge la temerpatura
static float readTempC() {
    uint16_t a = analogRead(AIN_TEMP010);
    return TS.gain * a + TS.off;
}

// Setup pin fisici
void setupIngressi() {
  for (int i = 0; i < NUM_INGRESSI; i++) {
    if (ingressi[i].pin != -1) {
      pinMode(ingressi[i].pin, INPUT_PULLUP);
    }
  }
}


// Aggiorna tutti gli ingressi (fisici, virtuali, I2C)
void aggiornaIngressi() {
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
  float tC = readTempC();
  // 2️⃣ ingresso virtuale temperatura
  ingressi[Ptemp].stato = (tC >= TS.Tmax);


  // 3️⃣ ingressi dallo slave I2C: ONOFF, POVER, RESET
  Wire.requestFrom(SLAVE_ADDR, 3);
  byte buffer[3];
  int i = 0;
  while (Wire.available() && i < 3) {
    buffer[i++] = Wire.read();
  }

  if (i == 3) { // se ricevo tutti e 3 i byte
    ingressi[ONOFF].stato = buffer[0];
    ingressi[POWER].stato = buffer[1];
    ingressi[RESETSW].stato = buffer[2];
  }
}


