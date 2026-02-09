# RemSens ESP32
**Remote Sensors & Actuators – Progetto di laboratorio**

Progetto didattico basato su **ESP32** per il controllo remoto di sensori e attuatori tramite **API HTTP** e **JSON**, con interfaccia web integrata.

Il progetto nasce come evoluzione dell’attività *RemoRoboRadar* e viene utilizzato per attività di laboratorio nelle classi di **Sistemi e Reti / Informatica**.

---

## 🎯 Obiettivi didattici

- Comprendere il funzionamento di un sistema **embedded connesso in rete**
- Progettare e utilizzare **API REST** su microcontrollore
- Gestire dati strutturati in **JSON**
- Integrare **sensori fisici** e **attuatori**
- Sviluppare una **web UI** minimale senza framework esterni

---

## 🧰 Hardware utilizzato

| Componente | Descrizione |
|-----------|------------|
| ESP32 | Microcontrollore principale |
| DHT11 | Sensore temperatura e umidità |
| HC-SR04 | Sensore di distanza a ultrasuoni |
| LDR | Sensore di luminosità (ADC) |
| LED RGB | Segnalazione visiva |
| Buzzer passivo | Segnalazione acustica / musica |
| Breadboard + jumper | Cablaggio |

---

## 🔌 Cablaggio (riassunto)

| Funzione | Pin ESP32 |
|--------|----------|
| DHT11 | GPIO dedicato |
| HC-SR04 TRIG | GPIO dedicato |
| HC-SR04 ECHO | GPIO dedicato |
| LDR (ADC) | GPIO 33 |
| LED Rosso | GPIO 21 |
| LED Verde | GPIO 19 |
| LED Blu | GPIO 18 |
| Buzzer | GPIO PWM |

> Tutti i componenti lavorano a **3.3 V**, compatibili con ESP32.

---

## 🌐 Funzionalità principali

### Web server integrato
- Pagina HTML generata direttamente dall’ESP32
- Layout centrato e responsive
- Aggiornamento automatico **solo del log** (AJAX / fetch)

### Sensori
- Temperatura e umidità (DHT11)
- Distanza media stabilizzata (HC-SR04)
- Luminosità espressa **in percentuale**

### Attuatori
- LED RGB controllabile via API
- Buzzer con:
  - beep singolo
  - riproduzione melodie tramite JSON

### Log di rete
- Tracciamento IP, endpoint e orario
- Ordinamento: richieste più recenti in cima

---

## 🔗 API disponibili

### `GET /state`
Restituisce lo stato completo del dispositivo in JSON.

```json
{
  "wifi": { "ssid": "...", "ip": "..." },
  "sensors": { "temperature": 23.1, "humidity": 55 },
  "light": { "adc": 1840, "percent": 62 }
}
```

---

### `GET /sensors`
Restituisce solo le letture correnti dei sensori.

---

### `GET /setLed?r=&g=&b=`
Imposta il colore del LED RGB (0–255).

Esempio:
```
/setLed?r=0&g=120&b=0
```

---

### `GET /beep?ms=&duty=`
Esegue un beep sul buzzer.

Esempio:
```
/beep?ms=120&duty=110
```

---

### `POST /playSong`
Invia una melodia al buzzer in formato JSON.

```json
{
  "gapMs": 20,
  "duty": 110,
  "melody": [
    [659, 150],
    [659, 150],
    [0, 150],
    [523, 300]
  ]
}
```

- `freq = 0` indica una pausa
- Riproduzione **non bloccante**

---

### `GET /stopSong`
Interrompe la riproduzione musicale.

---

### `GET /log`
Restituisce **solo l’HTML del log**, utilizzato per l’aggiornamento dinamico della pagina.

---

## 🖥 Script Python di esempio

Nella cartella `tools/` è presente uno script Python che:
- costruisce una melodia
- la invia all’ESP32 tramite JSON
- permette di testare la comunicazione client/server

---

## 📁 Struttura del progetto

```
RemSensESP32/
│
├── RemSensESP32.ino        # Sketch principale
├── wifi_config.h          # Credenziali WiFi (non versionare password reali)
├── README.md              # Documentazione
│
├── tools/
│   └── play_song.py       # Client Python di esempio
│
└── docs/
    └── schema_cablaggio

---

## 🔐 WiFi e sicurezza

Le credenziali WiFi sono conservate in un file separato (`wifi_config.h`) che **non deve contenere password reali nel repository pubblico**.

## 👨‍🏫 Attività proposte agli studenti

- Analisi delle API esistenti
- Estensione dell’endpoint `/state`
- Aggiunta di nuovi sensori o attuatori
- Creazione di melodie JSON
- Sviluppo di client Python personalizzati
- Uso di Git per lavoro collaborativo
- Scrittura di documentazione tecnica

---

## 📌 Stato del progetto

- Base stabile
- API funzionanti
- Interfaccia web operativa
- Progetto in **sviluppo continuo** durante il laboratorio

---

## 📜 Licenza

Progetto a **scopo didattico**.
Utilizzabile e modificabile liberamente per attività scolastiche.

---
