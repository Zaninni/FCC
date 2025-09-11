# FCC Cooling System Controller

Questo progetto contiene due sketch Arduino che implementano il controllo di un impianto di raffreddamento basato su una FSM non bloccante:

- **Opta AFX00002** (`OptaMaster/OPTA_Master_FSM_12LED.ino`): gestisce la logica di processo, le temporizzazioni e gli allarmi.
- **UNO R3** (`UnoHMI/UNO_R3_HMI_I2C_12LED.ino`): funge da interfaccia uomo‑macchina (HMI) tramite I²C e 12 LED di stato.

## Mappa I/O
| Sensore/Attuatore | Pin Opta | Descrizione |
|-------------------|---------:|-------------|
| Pressostato minima | DIN1 | `POL.minP_activeHigh` |
| Pressostato massima | DIN2 | `POL.maxP_activeHigh` |
| Flusso presente | DIN3 | `POL.flowPresent_activeHigh` |
| Fault chiller | DIN4 | `POL.chFault_activeHigh` |
| Temperatura 0‑10 V | A0 | letto con `readTempC()` |
| Power‑off manuale | DIN8 | interrompe immediatamente tutte le uscite |
| Relè riscaldatore | RELAY1 | `RLY_HEATER` |
| Relè pompa | RELAY2 | `RLY_PUMP` |
| Relè chiller | RELAY3 | `RLY_CHILLER` |
| Relè extra | RELAY4 | `RLY_EXTRA` |

## Stati della macchina a stati
La FSM dell’Opta è definita in `enum class FsmState` e comprende:

- **EMERGENZA** – attivata da `DIN8` o da allarmi, disattiva subito tutti i relè e richiede un reset manuale.
- **RESET** – spegne le uscite e azzera gli allarmi latched.
- **OFF_STATE** – tutte le uscite spente; accetta comandi di avvio, reset o pompaggio forzato.
- **ON_STATE** – avvia la pompa, poi il chiller dopo `TT.pumpPreMs`; il riscaldatore è abilitato solo in assenza di allarmi.
- **FORCED_PUMP_ONLY** – mantiene attiva solo la pompa finché la leva forzata è premuta.

## Sequenza di avvio e spegnimento
1. Con comando START, la pompa si accende subito; il chiller segue dopo `TT.pumpPreMs`.
2. Un arresto o un allarme spengono pompa e riscaldatore; il chiller resta attivo per `TT.chillerOffDelayMs` per post‑raffreddamento.

## Comunicazione I²C con HMI
L’Opta legge dall’UNO tre bit (START, FORZATO, RESET) e gli invia un frame LED con 12 bit ON e 12 bit BLINK.
L’UNO campiona leve e pulsante con antirimbalzo, aggiorna i LED e fornisce un heartbeat sul LED integrato.

Per maggiori dettagli consultare i commenti nei rispettivi sketch.
