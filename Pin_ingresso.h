#ifndef PIN_INGRESSO_H
#define PIN_INGRESSO_H

// Definizione dei pin fisici
constexpr uint8_t DIN_MINP     = A0;
constexpr uint8_t DIN_MAXP     = A1;
constexpr uint8_t DIN_FLOW_OK  = A2;
constexpr uint8_t DIN_CHFAULT  = A3;
constexpr uint8_t DIN_POWEROFF = A7;
constexpr uint8_t AIN_TEMP010 = A4;

constexpr byte SLAVE_ADDR = 8;

#endif
