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

#define DOUT_HEATER  D0
#define DOUT_PUMP    D1
#define DOUT_CHILLER D3
// funzione temperatura

// LETTURA STATO





#define ALL_OFF_CMD 255
#define REFRESH_ALL_LED 254
const unsigned long DEBOUNCE_MS = 30;

int currentLed = -1;               // -1 = nessun LED selezionato ancora
bool rawPrev = HIGH;               // con INPUT_PULLUP: riposo=HIGH
bool stablePrev = HIGH;
unsigned long lastChangePUMP_Ms = 0;

bool onoff_stat   = false;
bool onoff_prev   = false;

unsigned long timer_accensione = 0;
const signed long time_pump_on = 10000; //ms
const signed long time_chil_on = 30000; //ms

bool MasterAlarm = false;
bool startPumpAlarm = false;
bool startChilAlarm = false;

void setup() {
  analogReadResolution(12);
  Wire.begin();              // Master
  setupIngressi();
  Serial.begin(115200);

  
  //pinMode(LED_OUT_PUMP,OUTPUT);
  pinMode(DOUT_PUMP,OUTPUT);
  //pinMode(LED_OUT_CHILLER,OUTPUT);
  pinMode(DOUT_CHILLER,OUTPUT);
  //pinMode(LED_OUT_HEATER,OUTPUT);
  pinMode(DOUT_HEATER,OUTPUT);

   }

void loop() {

  aggiornaIngressi();      // aggiorna tutti gli ingressi
  sendLedCommand(255);
  aggiornaLed();          // aggiorna pin di uscita
  //sendLedCommand(255);

  unsigned long now = millis();

  //ad ogni giro nel loop mi verifico lo stato del master alarm e me lo segno
  startPumpAlarm = ingressi[Ptemp].stato || ingressi[Pmin].stato || ingressi[Pmax].stato;
  startChilAlarm = ingressi[Ptemp].stato || ingressi[Pmin].stato || ingressi[Pmax].stato || ingressi[Flow].stato;
  MasterAlarm    = ingressi[Ptemp].stato || ingressi[Pmin].stato || ingressi[Pmax].stato || ingressi[Flow].stato || ingressi[Chiller].stato;
  

  if (ingressi[POFF].stato) {
    Serial.println("Master Switch attivo!");
    if (ingressi[PUMP_OV].stato) {
      Serial.println("PUMP OVERRIDE attivo!");
      digitalWrite(DOUT_PUMP,HIGH);   //accendi pompa
      digitalWrite(DOUT_HEATER,LOW); //spegni heater
      digitalWrite(DOUT_CHILLER,LOW);//spegni chiller
      sendLedCommand(LED_OUT_PUMP); //accendi led stato pompa
      onoff_stat = false; //reinizializza la procedura di avvio in caso di cattivo utilizzo
    }
    else if (!ingressi[PUMP_OV].stato){
      Serial.println("PUMP OVERRIDE spento!");
      
      if (ingressi[ONOFF].stato) {
        Serial.println("ONOFF attivo!");
        //verifico che stato era prima
        onoff_prev = onoff_stat;
        if(!onoff_prev){
          //inizializzo il tempo perche sono appena entrato
          timer_accensione = now;
          Serial.println("Procedura Avvio Iniziata!");
        }
        onoff_stat = true;

        if (now - timer_accensione <= time_pump_on){
          if(!startPumpAlarm){
            Serial.println("Avvio Pompa");
            digitalWrite(DOUT_PUMP,HIGH);   //accendi pompa
            digitalWrite(DOUT_HEATER,LOW); //spegni heater
            digitalWrite(DOUT_CHILLER,LOW);//spegni chiller
            sendLedCommand(LED_OUT_PUMP); //accendi led stato pompa
          }
          else{
            Serial.println("Sequenza di avvio interrotta per errore startPumpAlarm");
            digitalWrite(DOUT_PUMP,LOW);   //spegni pompa
            digitalWrite(DOUT_HEATER,LOW); //spegni heater
            digitalWrite(DOUT_CHILLER,LOW);//spegni chiller
          }
        }
        else if (now - timer_accensione <= time_pump_on + time_chil_on){
          if(!startChilAlarm){
            Serial.println("Avvio Chiller");
            digitalWrite(DOUT_PUMP,HIGH);   //accendi pompa
            digitalWrite(DOUT_HEATER,LOW); //spegni heater
            digitalWrite(DOUT_CHILLER,HIGH); //accendi chiller
            sendLedCommand(LED_OUT_PUMP); //accendi led stato pompa
            delay(2);
            sendLedCommand(LED_OUT_CHILLER); //accendi led stato chiller
            delay(2);
          }
          else{
            Serial.println("Sequenza di avvio interrotta per errore startChilAlarm");
            digitalWrite(DOUT_PUMP,HIGH);   //accendi pompa
            digitalWrite(DOUT_HEATER,LOW); //spegni heater
            digitalWrite(DOUT_CHILLER,LOW);//spegni chiller
            sendLedCommand(LED_OUT_PUMP); //accendi led stato pompa
            delay(2);
          }
        }
        else if (now - timer_accensione > time_pump_on + time_chil_on){
          if(!MasterAlarm){
            Serial.println("Allarmi OK: Avvio Heater");
            digitalWrite(DOUT_PUMP,HIGH);   //accendi pompa
            digitalWrite(DOUT_HEATER,HIGH); //accendi heater
            digitalWrite(DOUT_CHILLER,HIGH);//accendi chiller
            sendLedCommand(LED_OUT_PUMP); //accendi led stato pompa
            delay(2);
            sendLedCommand(LED_OUT_CHILLER); //accendi led stato chiller
            delay(2);
            sendLedCommand(LED_OUT_HEATER); //accendi led stato chiller
            delay(2);
          }
          else if(MasterAlarm){
            Serial.println("Master Alarm! Attenzione!!!");
            digitalWrite(DOUT_HEATER,LOW); //spegni heater
            //in base a che allarme e' spegni anche pompa o chiller
            digitalWrite(DOUT_PUMP,HIGH);   //accendi pompa
            digitalWrite(DOUT_CHILLER,HIGH);//accendi chiller
            sendLedCommand(LED_OUT_PUMP); //accendi led stato pompa
            delay(2);
            sendLedCommand(LED_OUT_CHILLER); //accendi led stato chiller
            delay(2);
          }
        }
               
      }
      else if (!ingressi[ONOFF].stato){
        Serial.println("ONOFF spento!");
        onoff_stat = false;
        digitalWrite(DOUT_HEATER,LOW); //spegni heater
        //spegni chiller e pompa dopo 5 minuti di sicurezza
        //delay(5*60000);
        digitalWrite(DOUT_CHILLER,LOW);//spegni chiller
        digitalWrite(DOUT_PUMP,LOW);   //spegni pompa
        
      }
      
    }
    
  }
  else if (!ingressi[POFF].stato){
    Serial.println("Master Switch spento!");
    digitalWrite(DOUT_HEATER,LOW); //spegni heater
    digitalWrite(DOUT_CHILLER,LOW);//spegni chiller
    digitalWrite(DOUT_PUMP,LOW);   //spegni pompa
  }

  
  if (ingressi[RESETSW].stato) Serial.println("RESET attivo!");
  /*
  if (ingressi[Ptemp].stato) Serial.println("Temperatura alta!");
  if (ingressi[Pmin].stato) Serial.println("Pmin!");
  if (ingressi[Pmax].stato) Serial.println("Pmax!");
  if (ingressi[Flow].stato) Serial.println("Flow!");
  if (ingressi[Chiller].stato) Serial.println("Chiller!");
  */


  delay(500);






}
