#include <Wire.h>

// ================= CONFIG =================
const bool DEBUG_ENABLED = true;
const uint8_t UNO_ADDR = 0x12;

struct Timings {
  uint16_t scanMs      = 20;
  uint16_t heartbeatMs = 500;
  uint16_t debounceMs  = 30;
  uint32_t flowTimeoutMs       = 10000;   // entro 10 s deve esserci flusso
  uint32_t chillerCheckDelayMs = 20000;   // verifica fault chiller dopo 20 s
  uint32_t offChillerDelayMs   = 60000;   // spegnimento chiller dopo stop
  uint32_t offPumpDelayMs      = 180000;  // spegnimento pompa dopo stop
  uint32_t emergPumpChillerMs  = 15000;   // in emergenza, pompa/chiller OFF dopo 15 s
} TT;

struct TempScale {
  float gain = 100.0f/4095.0f;
  float off  = 0.0f;
  float Tmax = 50.0f;
  float hyst = 1.0f;
} TS;

struct DinPolarity {
  bool minP_activeHigh        = true; // DIN1
  bool maxP_activeHigh        = true; // DIN2
  bool flowPresent_activeHigh = true; // DIN3
  bool chFault_activeHigh     = true; // DIN4
  bool powerOff_activeHigh    = true; // DIN8
} POL;

// ================= PIN ======================
const uint8_t DIN_MINP     = 1;
const uint8_t DIN_MAXP     = 2;
const uint8_t DIN_FLOW_OK  = 3;
const uint8_t DIN_CHFAULT  = 4;
const uint8_t DIN_POWEROFF = 8;
const uint8_t AIN_TEMP010  = A0;

const uint8_t RLY_HEATER   = 1;
const uint8_t RLY_PUMP     = 2;
const uint8_t RLY_CHILLER  = 3;
const uint8_t RLY_EXTRA    = 4;

const uint8_t LED_BOARD    = LED_BUILTIN;

// ====== PAYLOAD HMI ======
struct __attribute__((packed)) UnoPayload {
  uint8_t  hmiBits;   // bit0:START, bit1:FORZ, bit2:RESET
  uint32_t ms;
};

struct __attribute__((packed)) LedFrame {
  uint16_t on_bits;
  uint16_t blink_bits;
};

enum LedIdx : uint8_t {
  L_STATE_ON=0, L_STATE_OFF,
  L_FORZATO,
  L_OUT_HEATER,
  L_OUT_PUMP,
  L_OUT_CHILLER,
  L_EMERGENCY_ACTIVE,
  L_AL_MINP,
  L_AL_MAXP,
  L_AL_NOFLOW,
  L_AL_CHFAULT,
  L_AL_TMAX
};

// ====== FSM ======
enum class FsmState : uint8_t { EMERGENZA, RESET, POWEROFF, OFF_STATE, ON_STATE, FORCED_PUMP_ONLY };

typedef struct { bool stable=false, last=false; uint32_t t=0; } DebDI;
DebDI diMinP, diMaxP, diFlowOK, diChFault, diPowerOff;

struct AlarmsLatched {
  bool minP=false, maxP=false, noFlow=false, chFault=false, temp=false;
} alLatched;
static inline bool anyAlarmLatched(){
  return alLatched.minP || alLatched.maxP || alLatched.noFlow || alLatched.chFault || alLatched.temp;
}

FsmState st = FsmState::OFF_STATE;

// stato uscite
bool pumpOn=false, chillerOn=false, heaterOn=false;

// tempi
uint32_t tScan=0, tHb=0;
uint32_t tPumpStart=0, tChillerStart=0;
uint32_t tStop=0; bool stopping=false;
uint32_t tEmerg=0; bool emergInit=false;

UnoPayload UNO{}; LedFrame LTX{0,0};

// ====== UTILITY ======
static inline bool readDI(DebDI& d, uint8_t pin, bool activeHigh){
  bool raw=digitalRead(pin); bool logical=activeHigh?raw:!raw;
  if (logical!=d.last){ if (millis()-d.t>=TT.debounceMs){ d.last=logical; d.t=millis(); } }
  else d.t=millis();
  d.stable=d.last; return d.stable;
}

static inline float readTempC(){ uint16_t a=analogRead(AIN_TEMP010); return TS.gain*a+TS.off; }

static inline void setRelays(bool heater,bool pump,bool chiller,bool extra=false){
  digitalWrite(RLY_HEATER, heater?HIGH:LOW);
  digitalWrite(RLY_PUMP, pump?HIGH:LOW);
  digitalWrite(RLY_CHILLER,chiller?HIGH:LOW);
  digitalWrite(RLY_EXTRA, extra?HIGH:LOW);
}
static inline void allRelaysOff(){ setRelays(false,false,false,false); }

static inline void hbTick(){ if (millis()-tHb>=TT.heartbeatMs){ tHb=millis(); digitalWrite(LED_BOARD,!digitalRead(LED_BOARD)); } }

static inline bool i2cReadUNO(UnoPayload& out){
  Wire.requestFrom((int)UNO_ADDR,(int)sizeof(UnoPayload));
  if (Wire.available()!=sizeof(UnoPayload)){ while(Wire.available())(void)Wire.read(); return false; }
  Wire.readBytes((char*)&out,sizeof(UnoPayload)); return true;
}
static inline void i2cWriteLeds(const LedFrame& frm){
  Wire.beginTransmission(UNO_ADDR); Wire.write((uint8_t*)&frm,sizeof(frm)); Wire.endTransmission();
}

static inline bool hmiStartOn(){ return (UNO.hmiBits>>0)&0x01; }
static inline bool hmiForced(){  return (UNO.hmiBits>>1)&0x01; }
static inline bool hmiReset(){   return (UNO.hmiBits>>2)&0x01; }

// ====== SETUP ======
void setup(){
  if (DEBUG_ENABLED){ Serial.begin(115200); delay(30); Serial.println(F("[OPTA] FSM 12 LED")); }
  pinMode(LED_BOARD,OUTPUT);
  pinMode(DIN_MINP,INPUT); pinMode(DIN_MAXP,INPUT); pinMode(DIN_FLOW_OK,INPUT);
  pinMode(DIN_CHFAULT,INPUT); pinMode(DIN_POWEROFF,INPUT); pinMode(AIN_TEMP010,INPUT);
  pinMode(RLY_HEATER,OUTPUT); pinMode(RLY_PUMP,OUTPUT); pinMode(RLY_CHILLER,OUTPUT); pinMode(RLY_EXTRA,OUTPUT);
  allRelaysOff();
  Wire.begin();
  Wire.setClock(50000); // I2C bus at 50 kHz
}

// ====== LOOP ======
void loop(){
  hbTick();
  if (millis()-tScan < TT.scanMs) return;
  tScan=millis();

  // ingresso digitali
  readDI(diMinP, DIN_MINP, POL.minP_activeHigh);
  readDI(diMaxP, DIN_MAXP, POL.maxP_activeHigh);
  readDI(diFlowOK, DIN_FLOW_OK, POL.flowPresent_activeHigh);
  readDI(diChFault, DIN_CHFAULT, POL.chFault_activeHigh);
  readDI(diPowerOff, DIN_POWEROFF, POL.powerOff_activeHigh);

  bool unoOK = i2cReadUNO(UNO);
  float tC = readTempC();

  // gestione stato manuale POWEROFF
  if (diPowerOff.stable){ st = FsmState::POWEROFF; }
  else if (st==FsmState::POWEROFF){ st = FsmState::OFF_STATE; }

  // ===== FSM =====
  switch(st){
    case FsmState::EMERGENZA: {
      if (!emergInit){ tEmerg=millis(); emergInit=true; }
      digitalWrite(RLY_HEATER,LOW); heaterOn=false;
      if (millis()-tEmerg >= TT.emergPumpChillerMs){
        digitalWrite(RLY_PUMP,LOW);  pumpOn=false;
        digitalWrite(RLY_CHILLER,LOW); chillerOn=false;
      }
      bool procAlarm = diMinP.stable || diMaxP.stable || (pumpOn && !diFlowOK.stable) ||
                       (chillerOn && diChFault.stable) || (tC>=TS.Tmax);
      if (!procAlarm && unoOK && hmiReset()) { st=FsmState::RESET; }
    } break;

    case FsmState::RESET:
      allRelaysOff(); pumpOn=chillerOn=heaterOn=false; stopping=false; emergInit=false;
      alLatched = {}; st = FsmState::OFF_STATE; break;

    case FsmState::POWEROFF:
      allRelaysOff(); pumpOn=chillerOn=heaterOn=false; stopping=false; emergInit=false; break;

    case FsmState::OFF_STATE: {
      if (stopping){
        uint32_t dt = millis()-tStop;
        if (dt >= TT.offChillerDelayMs && chillerOn){ digitalWrite(RLY_CHILLER,LOW); chillerOn=false; }
        if (dt >= TT.offPumpDelayMs && pumpOn){ digitalWrite(RLY_PUMP,LOW); pumpOn=false; stopping=false; }
        bool aMinP = pumpOn && diMinP.stable;
        bool aMaxP = pumpOn && diMaxP.stable;
        bool aNoFlow = pumpOn && !diFlowOK.stable;
        bool aChFault = chillerOn && diChFault.stable;
        bool aTemp = tC>=TS.Tmax;
        if ((aMinP||aMaxP||aNoFlow||aChFault||aTemp) && !stopping==false){
          if (aMinP) alLatched.minP=true;
          if (aMaxP) alLatched.maxP=true;
          if (aNoFlow) alLatched.noFlow=true;
          if (aChFault) alLatched.chFault=true;
          if (aTemp) alLatched.temp=true;
          st=FsmState::EMERGENZA; }
      } else {
        allRelaysOff(); pumpOn=chillerOn=heaterOn=false;
        if (unoOK && hmiForced() && !anyAlarmLatched()) st = FsmState::FORCED_PUMP_ONLY;
        else if (unoOK && hmiStartOn() && !anyAlarmLatched()) {
          if (diMinP.stable){ alLatched.minP=true; st=FsmState::EMERGENZA; }
          else if (diMaxP.stable){ alLatched.maxP=true; st=FsmState::EMERGENZA; }
          else {
            st = FsmState::ON_STATE; pumpOn=chillerOn=heaterOn=false; tPumpStart=millis();
            digitalWrite(RLY_PUMP,HIGH); pumpOn=true; // start pump immediately
          }
        }
        if (unoOK && hmiReset()) alLatched = {};
      }
    } break;

    case FsmState::ON_STATE: {
      // avvio sequenziale
      if (!chillerOn && millis()-tPumpStart >= TT.flowTimeoutMs){
        if (!diFlowOK.stable){ alLatched.noFlow=true; st=FsmState::EMERGENZA; break; }
        digitalWrite(RLY_CHILLER,HIGH); chillerOn=true; tChillerStart=millis();
      }
      if (chillerOn && !heaterOn && millis()-tChillerStart >= TT.chillerCheckDelayMs){
        if (diChFault.stable){ alLatched.chFault=true; st=FsmState::EMERGENZA; break; }
        digitalWrite(RLY_HEATER,HIGH); heaterOn=true;
      }
      // monitor allarmi durante funzionamento
      bool aMinP = diMinP.stable;
      bool aMaxP = diMaxP.stable;
      bool aTemp = tC>=TS.Tmax;
      bool aNoFlow = (millis()-tPumpStart >= TT.flowTimeoutMs) && !diFlowOK.stable;
      bool aChFault = chillerOn && diChFault.stable;
      if (aMinP||aMaxP||aNoFlow||aChFault||aTemp){
        if (aMinP) alLatched.minP=true;
        if (aMaxP) alLatched.maxP=true;
        if (aNoFlow) alLatched.noFlow=true;
        if (aChFault) alLatched.chFault=true;
        if (aTemp) alLatched.temp=true;
        st=FsmState::EMERGENZA; break;
      }
      if (!unoOK || !hmiStartOn()){
        digitalWrite(RLY_HEATER,LOW); heaterOn=false;
        stopping=true; tStop=millis(); st=FsmState::OFF_STATE;
      }
    } break;

    case FsmState::FORCED_PUMP_ONLY:
      digitalWrite(RLY_HEATER,LOW); digitalWrite(RLY_CHILLER,LOW); digitalWrite(RLY_PUMP,HIGH);
      pumpOn=true; chillerOn=false; heaterOn=false;
      if (!(unoOK && hmiForced())){ digitalWrite(RLY_PUMP,LOW); pumpOn=false; st=FsmState::OFF_STATE; }
      break;
  }

  // ===== LED FRAME =====
  bool sMinP=diMinP.stable, sMaxP=diMaxP.stable;
  bool sFlow=diFlowOK.stable, sCh=diChFault.stable;
  bool sTemp=(tC>=TS.Tmax);
  uint16_t B=0, BL=0;
  if (st==FsmState::ON_STATE) B |= (1u<<L_STATE_ON); else B |= (1u<<L_STATE_OFF);
  if (heaterOn)  B |= (1u<<L_OUT_HEATER);
  if (pumpOn)    B |= (1u<<L_OUT_PUMP);
  if (chillerOn) B |= (1u<<L_OUT_CHILLER);
  if (st==FsmState::EMERGENZA){
    if (alLatched.minP)   B |= (1u<<L_AL_MINP);
    if (alLatched.maxP)   B |= (1u<<L_AL_MAXP);
    if (alLatched.noFlow) B |= (1u<<L_AL_NOFLOW);
    if (alLatched.chFault)B |= (1u<<L_AL_CHFAULT);
    if (alLatched.temp)   B |= (1u<<L_AL_TMAX);
    BL |= (1u<<L_EMERGENCY_ACTIVE);
  } else {
    if (sMinP)   B |= (1u<<L_AL_MINP);
    if (sMaxP)   B |= (1u<<L_AL_MAXP);
    if (!sFlow)  B |= (1u<<L_AL_NOFLOW);
    if (sCh)     B |= (1u<<L_AL_CHFAULT);
    if (sTemp)   B |= (1u<<L_AL_TMAX);
  }
  if (st==FsmState::FORCED_PUMP_ONLY) B |= (1u<<L_FORZATO);
  LTX.on_bits=B & 0x0FFF; LTX.blink_bits=BL & 0x0FFF; i2cWriteLeds(LTX);

  if (DEBUG_ENABLED){
    Serial.print(F("[OPTA] S="));
    switch(st){
      case FsmState::EMERGENZA: Serial.print("EMERG"); break;
      case FsmState::RESET: Serial.print("RESET"); break;
      case FsmState::POWEROFF: Serial.print("POWEROFF"); break;
      case FsmState::OFF_STATE: Serial.print("OFF"); break;
      case FsmState::ON_STATE: Serial.print("ON"); break;
      case FsmState::FORCED_PUMP_ONLY: Serial.print("FORZATO"); break;
    }
    Serial.print(F(" MinP=")); Serial.print(sMinP);
    Serial.print(F(" MaxP=")); Serial.print(sMaxP);
    Serial.print(F(" Flow=")); Serial.print(sFlow);
    Serial.print(F(" ChFl=")); Serial.print(sCh);
    Serial.print(F(" Temp=")); Serial.print(tC,1);
    Serial.print(F(" Lat=")); Serial.println(anyAlarmLatched());
  }
}
