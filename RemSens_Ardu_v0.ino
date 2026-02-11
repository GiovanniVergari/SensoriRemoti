/*
  RemSens_ESP32_Web_JSON
  ====================
  Evoluzione del progetto RemoRoboRadar per nuovo hardware.

  Architettura mantenuta:
  - ESP32 WebServer
  - Multi WiFi con IP statico
  - NTP (fuso orario Italia)
  - Log accessi client

  Nuovo hardware:
  - HC-SR04 (distanza)
  - DHT11 (temperatura/umidità)
  - LDR analogico
  - LED RGB (PWM)
  - Buzzer passivo (PWM)

  API principali:
  - /            -> Pagina HTML descrittiva
  - /state       -> JSON stato completo
  - /sensors     -> JSON sensori
  - /setSafeDistance?value=
  - /setLightThreshold?value=
  - /setLed?r=&g=&b=
  - /beep?ms=&duty=
*/

#include <WiFi.h>
#include <WebServer.h>
#include <time.h>
#include <DHT.h>
#include <ArduinoJson.h>

/*
   FILE SEPARATO PER CREDENZIALI WIFI
   -------------------------------
   Per evitare di inserire SSID/password nel file principale (e per facilitarne la modifica),
   le reti WiFi sono definite in un file esterno: "wifi_secrets.h".

   Creare, nella stessa cartella dello sketch, un file chiamato:
     wifi_secrets.h

   e inserire al suo interno il contenuto fornito nelle istruzioni (vedi messaggio in chat).
*/
#include "wifi_secrets.h"

/* ===============================
   CONFIGURAZIONE WIFI
   ===============================

   Nota:
   - wifiNetworks[] e wifiCount sono definiti in wifi_secrets.h
   - currentSSID rimane qui perché è stato applicativo.
*/

String currentSSID = "Nessuna";

/* ===============================
   OGGETTI GLOBALI
   =============================== */

WebServer server(80);

/* ===============================
   PIN CONFIGURATION
   =============================== */

#define PIN_DHT      27
#define PIN_TRIG     26
#define PIN_ECHO     25
#define PIN_LDR      33

#define PIN_LED_R    18
#define PIN_LED_G    19
#define PIN_LED_B    21

#define PIN_BUZZER   14

/* ===============================
   SENSORI
   =============================== */

#define DHTTYPE DHT11
DHT dht(PIN_DHT, DHTTYPE);

/* ===============================
   STATO APPLICATIVO
   =============================== */

int safeDistanceCm = 20;
int lightThresholdRaw = 1500;   // Soglia in valori ADC (0..4095)

/*
   LDR in percentuale
   ------------------
   L'ESP32 legge l'ADC a 12 bit: 0..4095.
   Convertiamo in percentuale: 0..100.

   NOTA DIDATTICA:
   - Se nel vostro cablaggio il valore cresce al diminuire della luce (o viceversa),
     potete invertire la percentuale scambiando 0 e 100 nella formula.
*/
const int LDR_ADC_MIN = 0;
const int LDR_ADC_MAX = 4095;

int ledR = 0;
int ledG = 0;
int ledB = 0;

/* ===============================
   PLAYER MUSICALE (BUZZER PASSIVO)
   ===============================

   Obiettivo:
   - ricevere da rete una sequenza di note (frequenza + durata)
   - suonare senza bloccare il web server (player non bloccante)

   Formato consigliato (POST /playSong):

   {
     "gapMs": 20,
     "duty": 110,
     "melody": [
       [659, 150],
       [659, 150],
       [0,   150]
     ]
   }

   Regole:
   - freq = 0 significa pausa
   - durata in millisecondi
*/

struct NoteEvent {
  int freq;
  int ms;
};

const int SONG_MAX_EVENTS = 256;
NoteEvent songEvents[SONG_MAX_EVENTS];
int songLen = 0;
int songPos = 0;

bool songPlaying = false;
bool songInGap = false;

unsigned long noteDeadlineMs = 0;

int songGapMs = 20;         // pausa tra note
int buzzerDuty = 110;       // intensità PWM 0..255

String lastSongIP = "Nessuno";
String lastSongStatus = "Nessuna";

unsigned long uptimeStart;

struct ClientInfo {
  String ip;
  String endpoint;
  String time;
};

ClientInfo clients[10];
int clientCount = 0;

/* ===============================
   FUNZIONI DI SUPPORTO
   =============================== */

String getDateTime()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "NTP_ERROR";
  }
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(buffer);
}

void logClient(String ip, String endpoint)
{
  String now = getDateTime();

  for (int i = 0; i < clientCount; i++) {
    if (clients[i].ip == ip) {
      clients[i].endpoint = endpoint;
      clients[i].time = now;
      return;
    }
  }

  if (clientCount < 10) {
    clients[clientCount].ip = ip;
    clients[clientCount].endpoint = endpoint;
    clients[clientCount].time = now;
    clientCount++;
  }
}

/* ===============================
   PWM (ESP32 core 3.x)
   =============================== */

void initPwm()
{
  ledcAttach(PIN_LED_R, 5000, 8);
  ledcAttach(PIN_LED_G, 5000, 8);
  ledcAttach(PIN_LED_B, 5000, 8);
  ledcAttach(PIN_BUZZER, 2000, 8);
}

void setLedRgb(int r, int g, int b)
{
  if (r < 0) r = 0;
  if (g < 0) g = 0;
  if (b < 0) b = 0;
  if (r > 255) r = 255;
  if (g > 255) g = 255;
  if (b > 255) b = 255;

  ledR = r;
  ledG = g;
  ledB = b;

  ledcWrite(PIN_LED_R, r);
  ledcWrite(PIN_LED_G, g);
  ledcWrite(PIN_LED_B, b);
}

void beepMs(int ms, int duty)
{
  if (duty < 0) duty = 0;
  if (duty > 255) duty = 255;

  ledcWrite(PIN_BUZZER, duty);
  delay(ms);
  ledcWrite(PIN_BUZZER, 0);
}

void buzzerSilence()
{
  /*
     Silenzio esplicito:
     - ferma la frequenza
     - azzera il duty
  */
  ledcWriteTone(PIN_BUZZER, 0);
  ledcWrite(PIN_BUZZER, 0);
}

void buzzerTone(int freqHz, int duty)
{
  /*
     Riproduce una frequenza sul buzzer.
     Nota:
     - freqHz <= 0 -> pausa
  */

  if (duty < 0) duty = 0;
  if (duty > 255) duty = 255;

  if (freqHz <= 0) {
    buzzerSilence();
    return;
  }

  ledcWriteTone(PIN_BUZZER, freqHz);
  ledcWrite(PIN_BUZZER, duty);
}

void stopSong()
{
  songPlaying = false;
  songInGap = false;
  songLen = 0;
  songPos = 0;
  noteDeadlineMs = 0;

  buzzerSilence();
}

void startSong(int len)
{
  songLen = len;
  songPos = 0;
  songPlaying = true;
  songInGap = false;

  noteDeadlineMs = 0;
}

void musicTick()
{
  /*
     Player non bloccante:
     - gestisce note e pause usando millis()
     - permette al server di rispondere mentre suona
  */

  if (!songPlaying) {
    return;
  }

  unsigned long now = millis();

  if (noteDeadlineMs == 0) {
    /* Avvio immediato della prima nota */
    noteDeadlineMs = now;
  }

  if (now < noteDeadlineMs) {
    return;
  }

  /* Se siamo in una "gap" tra note, passiamo alla prossima nota */
  if (songInGap) {
    songInGap = false;
    buzzerSilence();

    if (songPos >= songLen) {
      stopSong();
      lastSongStatus = "Completata";
      return;
    }

    /* Avvio nota */
    int freq = songEvents[songPos].freq;
    int dur = songEvents[songPos].ms;

    buzzerTone(freq, buzzerDuty);

    if (dur < 0) {
      dur = 0;
    }

    noteDeadlineMs = now + (unsigned long)dur;
    songPos++;

    return;
  }

  /* Fine nota: entra nella pausa tra note */
  buzzerSilence();
  songInGap = true;

  if (songGapMs < 0) {
    songGapMs = 0;
  }

  noteDeadlineMs = now + (unsigned long)songGapMs;
}

/* ===============================
   SENSORI
   =============================== */

float readDistanceCm()
{
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  long duration = pulseIn(PIN_ECHO, HIGH, 30000);
  if (duration == 0) return -1;
  return duration / 58.0;
}

int clampInt(int value, int minValue, int maxValue)
{
  if (value < minValue) {
    return minValue;
  }

  if (value > maxValue) {
    return maxValue;
  }

  return value;
}

int ldrPercentFromRaw(int raw)
{
  /*
     Converte una lettura ADC (0..4095) in percentuale (0..100)
     - raw viene prima "clampato" per sicurezza.
     - percentuale arrotondata all'intero.
  */

  int clamped = clampInt(raw, LDR_ADC_MIN, LDR_ADC_MAX);

  float ratio = (float)clamped / (float)(LDR_ADC_MAX - LDR_ADC_MIN);
  float percentFloat = ratio * 100.0f;

  int percentInt = (int)(percentFloat + 0.5f);
  percentInt = clampInt(percentInt, 0, 100);

  return percentInt;
}

/* ===============================
   API HANDLER
   =============================== */

void handleRoot()
{
  /*
     Pagina HTML principale
     ---------------------
     Requisiti didattici:
     - contenuto centrato
     - stato sensori in evidenza
     - layout a due colonne:
         sinistra: API + descrizione + esempio
         destra: log connessioni (ultime in cima)

     Aggiornamento:
     - refresh totale DISABILITATO
     - aggiornamento SOLO del log via /log ogni 5 secondi
  */

  String ip = server.client().remoteIP().toString();
  logClient(ip, "/");

  /* Letture sensori per mostrare lo stato attuale */
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  float d = readDistanceCm();
  int ldrRaw = analogRead(PIN_LDR);
  int ldrPct = ldrPercentFromRaw(ldrRaw);

  String tStr;
  String hStr;

  if (isnan(t)) {
    tStr = "n/d";
  } else {
    tStr = String(t, 1) + " &deg;C";
  }

  if (isnan(h)) {
    hStr = "n/d";
  } else {
    hStr = String(h, 1) + " %";
  }

  String dStr;
  if (d < 0) {
    dStr = "timeout";
  } else {
    dStr = String(d, 1) + " cm";
  }

  String page = "<!DOCTYPE html><html lang='it'><head>";
  page += "<meta charset='utf-8'>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  page += "<title>RemSens ESP32</title>";

  /* Stile inline, semplice e robusto */
  page += "<style>";
  page += "body{font-family:Arial,Helvetica,sans-serif;margin:0;padding:0;background:#f4f6f8;}";
  page += ".outer{display:flex;justify-content:center;padding:18px;}";
  page += ".container{width:100%;max-width:980px;text-align:center;}";
  page += ".card{background:#fff;border:1px solid #d9dde3;border-radius:10px;padding:14px;margin-bottom:14px;box-shadow:0 1px 4px rgba(0,0,0,0.06);}";
  page += ".grid{display:flex;gap:14px;align-items:stretch;}";
  page += ".col{flex:1;}";
  page += ".title{font-size:22px;margin:0 0 8px 0;}";
  page += ".sub{margin:0 0 10px 0;color:#444;}";
  page += ".kv{display:inline-block;text-align:left;}";
  page += ".kv table{border-collapse:collapse;}";
  page += ".kv td{padding:4px 10px;border-bottom:1px solid #eef1f5;}";
  page += ".kv td:first-child{font-weight:bold;color:#222;}";
  page += ".api{margin:10px 0 0 0;text-align:left;}";
  page += ".api h3{margin:10px 0 6px 0;font-size:16px;}";
  page += ".api p{margin:0 0 6px 0;color:#333;}";
  page += ".code{font-family:Consolas,Monaco,monospace;background:#f1f3f5;border:1px solid #e2e6ea;border-radius:8px;padding:8px;overflow:auto;}";
  page += ".log table{width:100%;border-collapse:collapse;text-align:left;}";
  page += ".log th,.log td{padding:6px 8px;border-bottom:1px solid #eef1f5;font-size:13px;}";
  page += ".badge{display:inline-block;padding:2px 8px;border-radius:12px;background:#eef6ff;border:1px solid #d7e8ff;font-size:12px;}";
  page += "@media(max-width:820px){.grid{flex-direction:column;}}";
  page += "</style></head><body>";

  page += "<div class='outer'><div class='container'>";

  /* HEADER */
  page += "<div class='card'>";
  page += "<h1 class='title'>RemSens ESP32</h1>";
  page += "<p class='sub'>Aggiornamento log ogni 5 secondi (senza ricaricare la pagina)</p>";
  page += "<div class='kv'><table>";
  page += "<tr><td>SSID</td><td>" + currentSSID + "</td></tr>";
  page += "<tr><td>IP ESP32</td><td>" + WiFi.localIP().toString() + "</td></tr>";
  page += "<tr><td>Data/Ora</td><td>" + getDateTime() + "</td></tr>";
  page += "</table></div>";
  page += "</div>";

  /* STATO SENSORI */
  page += "<div class='card'>";
  page += "<h2 class='title' style='font-size:18px;'>Stato attuale sensori</h2>";
  page += "<div class='kv'><table>";
  page += "<tr><td>DHT11 - Temperatura</td><td>" + tStr + "</td></tr>";
  page += "<tr><td>DHT11 - Umidit&agrave;</td><td>" + hStr + "</td></tr>";
  page += "<tr><td>HC-SR04 - Distanza</td><td>" + dStr + "</td></tr>";
  page += "<tr><td>LDR - ADC</td><td>" + String(ldrRaw) + "</td></tr>";
  page += "<tr><td>LDR - Percentuale</td><td><span class='badge'>" + String(ldrPct) + "%</span></td></tr>";
  page += "</table></div>";
  page += "</div>";

  /* DUE COLONNE */
  page += "<div class='grid'>";

  /* COLONNA SINISTRA: API */
  page += "<div class='col card api'>";
  page += "<h2 class='title' style='font-size:18px;'>API disponibili</h2>";

  page += "<h3>/state</h3>";
  page += "<p>Restituisce lo stato completo del dispositivo in JSON (rete, sensori, soglie, LED).</p>";
  page += "<div class='code'>GET http://" + WiFi.localIP().toString() + "/state</div>";

  page += "<h3>/sensors</h3>";
  page += "<p>Restituisce solo le letture correnti dei sensori (JSON).</p>";
  page += "<div class='code'>GET http://" + WiFi.localIP().toString() + "/sensors</div>";

  page += "<h3>/setSafeDistance?value=</h3>";
  page += "<p>Imposta la soglia di distanza (cm) per l'attivit&agrave;.</p>";
  page += "<div class='code'>GET http://" + WiFi.localIP().toString() + "/setSafeDistance?value=30</div>";

  page += "<h3>/setLightThreshold?value=</h3>";
  page += "<p>Imposta la soglia luce in valori ADC (0..4095).</p>";
  page += "<div class='code'>GET http://" + WiFi.localIP().toString() + "/setLightThreshold?value=1500</div>";

  page += "<h3>/setLed?r=&amp;g=&amp;b=</h3>";
  page += "<p>Imposta il colore del LED RGB (0..255 per ciascun canale).</p>";
  page += "<div class='code'>GET http://" + WiFi.localIP().toString() + "/setLed?r=0&amp;g=120&amp;b=0</div>";

  page += "<h3>/beep?ms=&amp;duty=</h3>";
  page += "<p>Esegue un beep con durata (ms) e intensit&agrave; PWM (duty 0..255).</p>";
  page += "<div class='code'>GET http://" + WiFi.localIP().toString() + "/beep?ms=120&amp;duty=110</div>";

  page += "<h3>/playSong (POST JSON)</h3>";
  page += "<p>Invia una sequenza di note al buzzer. freq=0 significa pausa. Il dispositivo suona senza bloccare il server.</p>";
  page += "<div class='code'>POST http://" + WiFi.localIP().toString() + "/playSong<br>Body: {\"gapMs\":20,\"duty\":110,\"melody\":[[659,150],[659,150],[0,150]]}</div>";

  page += "<h3>/stopSong</h3>";
  page += "<p>Interrompe la riproduzione corrente.</p>";
  page += "<div class='code'>GET http://" + WiFi.localIP().toString() + "/stopSong</div>";

  page += "</div>";

  /* COLONNA DESTRA: LOG (AGGIORNABILE) */
  page += "<div class='col card log'>";
  page += "<h2 class='title' style='font-size:18px;'>Log connessioni</h2>";
  page += "<p class='sub' style='margin-top:0;'>Ultime richieste in cima</p>";

  page += "<div id='logArea'>";
  page += "<table><tr><th>IP</th><th>Endpoint</th><th>Orario</th></tr>";

  if (clientCount == 0) {
    page += "<tr><td colspan='3'>Nessuna richiesta registrata</td></tr>";
  } else {
    for (int i = clientCount - 1; i >= 0; i--) {
      page += "<tr>";
      page += "<td>" + clients[i].ip + "</td>";
      page += "<td>" + clients[i].endpoint + "</td>";
      page += "<td>" + clients[i].time + "</td>";
      page += "</tr>";
    }
  }

  page += "</table>";
  page += "</div>";
  page += "</div>";

  page += "</div>"; /* grid */

  page += "</div></div>"; /* container + outer */

  page += "<script>";
  page += "function updateLog(){";
  page += "  fetch('/log').then(function(r){return r.text();}).then(function(html){";
  page += "    var el = document.getElementById('logArea');";
  page += "    if(el){ el.innerHTML = html; }";
  page += "  }).catch(function(e){ console.log(e); });";
  page += "}";
  page += "setInterval(updateLog, 5000);";
  page += "</script>";

  page += "</body></html>";

  server.send(200, "text/html", page);
}

void handleLog()
{
  /*
     Restituisce SOLO l'HTML del contenuto log.
     IMPORTANTE:
     - NON chiamare logClient() qui, altrimenti il log si auto-alimenta.
  */

  String html = "";
  html += "<table><tr><th>IP</th><th>Endpoint</th><th>Orario</th></tr>";

  if (clientCount == 0) {
    html += "<tr><td colspan='3'>Nessuna richiesta registrata</td></tr>";
  } else {
    for (int i = clientCount - 1; i >= 0; i--) {
      html += "<tr>";
      html += "<td>" + clients[i].ip + "</td>";
      html += "<td>" + clients[i].endpoint + "</td>";
      html += "<td>" + clients[i].time + "</td>";
      html += "</tr>";
    }
  }

  html += "</table>";

  server.send(200, "text/html", html);
}


void handleSensors()
{
  String ip = server.client().remoteIP().toString();
  logClient(ip, "/sensors");

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  float d = readDistanceCm();
  int ldrRaw = analogRead(PIN_LDR);
  int ldrPct = ldrPercentFromRaw(ldrRaw);

  String json = "{";

  /* DHT (null se non disponibile) */
  if (isnan(t)) {
    json += "\"temperatureC\":null,";
  } else {
    json += "\"temperatureC\":" + String(t, 1) + ",";
  }

  if (isnan(h)) {
    json += "\"humidityPct\":null,";
  } else {
    json += "\"humidityPct\":" + String(h, 1) + ",";
  }

  /* HC-SR04 */
  json += "\"distanceCm\":" + String(d, 1) + ",";

  /* LDR */
  json += "\"ldrRaw\":" + String(ldrRaw) + ",";
  json += "\"ldrPercent\":" + String(ldrPct);

  json += "}";

  server.send(200, "application/json", json);
}

void handleState()
{
  String ip = server.client().remoteIP().toString();
  logClient(ip, "/state");

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  float d = readDistanceCm();
  int ldrRaw = analogRead(PIN_LDR);
  int ldrPct = ldrPercentFromRaw(ldrRaw);

  String json = "{";

  /* META */
  json += "\"meta\":{";
  json += "\"device\":\"RemSensESP32\",";
  json += "\"datetime\":\"" + getDateTime() + "\"";
  json += "},";

  /* NETWORK */
  json += "\"network\":{";
  json += "\"ssid\":\"" + currentSSID + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "},";

  /* SENSORS */
  json += "\"sensors\":{";

  if (isnan(t)) {
    json += "\"temperatureC\":null,";
  } else {
    json += "\"temperatureC\":" + String(t, 1) + ",";
  }

  if (isnan(h)) {
    json += "\"humidityPct\":null,";
  } else {
    json += "\"humidityPct\":" + String(h, 1) + ",";
  }

  json += "\"distanceCm\":" + String(d, 1) + ",";
  json += "\"ldrRaw\":" + String(ldrRaw) + ",";
  json += "\"ldrPercent\":" + String(ldrPct);

  json += "},";

  /* CONFIG */
  json += "\"config\":{";
  json += "\"safeDistanceCm\":" + String(safeDistanceCm) + ",";
  json += "\"lightThresholdRaw\":" + String(lightThresholdRaw) + ",";

  /* Soglia luce espressa anche in percentuale (utile per didattica) */
  json += "\"lightThresholdPercent\":" + String(ldrPercentFromRaw(lightThresholdRaw));

  json += "},";

  /* LED */
  json += "\"led\":{";
  json += "\"r\":" + String(ledR) + ",";
  json += "\"g\":" + String(ledG) + ",";
  json += "\"b\":" + String(ledB);
  json += "}";

  json += "}";

  server.send(200, "application/json", json);
}

void handleSetSafeDistance()
{
  String ip = server.client().remoteIP().toString();
  logClient(ip, "/setSafeDistance");

  if (server.hasArg("value")) {
    int newValue = server.arg("value").toInt();

    if (newValue > 0) {
      safeDistanceCm = newValue;
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "Valore non valido");
    }
  } else {
    server.send(400, "text/plain", "Parametro value mancante");
  }
}

void handleSetLightThreshold()

{
  if (server.hasArg("value")) {
    lightThresholdRaw = server.arg("value").toInt();
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Parametro value mancante");
  }
}

void handleSetLed()
{
  String ip = server.client().remoteIP().toString();
  logClient(ip, "/setLed");

  if (!server.hasArg("r") || !server.hasArg("g") || !server.hasArg("b")) {
    server.send(400, "text/plain", "Parametri mancanti: r, g, b");
    return;
  }

  int r = server.arg("r").toInt();
  int g = server.arg("g").toInt();
  int b = server.arg("b").toInt();

  setLedRgb(r, g, b);
  server.send(200, "text/plain", "LED aggiornato");
}

void handleBeep()
{
  String ip = server.client().remoteIP().toString();
  logClient(ip, "/beep");

  if (!server.hasArg("ms") || !server.hasArg("duty")) {
    server.send(400, "text/plain", "Parametri mancanti: ms, duty");
    return;
  }

  int ms = server.arg("ms").toInt();
  int duty = server.arg("duty").toInt();

  beepMs(ms, duty);
  server.send(200, "text/plain", "Beep eseguito");
}

void handleStopSong()
{
  String ip = server.client().remoteIP().toString();
  logClient(ip, "/stopSong");

  stopSong();
  lastSongIP = ip;
  lastSongStatus = "Stop richiesto";

  server.send(200, "application/json", "{\"ok\":true,\"status\":\"stopped\"}");
}

void handlePlaySong()
{
  /*
     POST /playSong
     Content-Type: application/json

     Esempio body:
     {
       "gapMs": 20,
       "duty": 110,
       "melody": [ [659,150], [659,150], [0,150] ]
     }
  */

  String ip = server.client().remoteIP().toString();
  logClient(ip, "/playSong");

  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Metodo non consentito: usare POST");
    return;
  }

  String body = server.arg("plain");
  if (body.length() == 0) {
    server.send(400, "text/plain", "Body vuoto: inviare JSON" );
    return;
  }

  /* Se una canzone è in corso, la interrompiamo e carichiamo la nuova */
  stopSong();

  StaticJsonDocument<8192> doc;
  DeserializationError err = deserializeJson(doc, body);

  if (err) {
    server.send(400, "text/plain", "JSON non valido" );
    lastSongIP = ip;
    lastSongStatus = "Errore JSON";
    return;
  }

  /* Parametri opzionali */
  if (doc.containsKey("gapMs")) {
    int g = doc["gapMs"].as<int>();
    if (g >= 0 && g <= 500) {
      songGapMs = g;
    }
  }

  if (doc.containsKey("duty")) {
    int d = doc["duty"].as<int>();
    if (d >= 0 && d <= 255) {
      buzzerDuty = d;
    }
  }

  if (!doc.containsKey("melody")) {
    server.send(400, "text/plain", "JSON privo di campo melody" );
    lastSongIP = ip;
    lastSongStatus = "Campo melody mancante";
    return;
  }

  JsonArray melody = doc["melody"].as<JsonArray>();
  if (melody.isNull()) {
    server.send(400, "text/plain", "Campo melody non è un array" );
    lastSongIP = ip;
    lastSongStatus = "melody non array";
    return;
  }

  int count = 0;

  for (JsonVariant v : melody) {
    if (count >= SONG_MAX_EVENTS) {
      break;
    }

    if (!v.is<JsonArray>()) {
      continue;
    }

    JsonArray pair = v.as<JsonArray>();
    if (pair.size() < 2) {
      continue;
    }

    int freq = pair[0].as<int>();
    int ms = pair[1].as<int>();

    /* Sanificazione */
    if (freq < 0) {
      freq = 0;
    }

    if (ms < 0) {
      ms = 0;
    }

    if (ms > 10000) {
      ms = 10000;
    }

    songEvents[count].freq = freq;
    songEvents[count].ms = ms;
    count++;
  }

  if (count == 0) {
    server.send(400, "text/plain", "Nessuna nota valida in melody" );
    lastSongIP = ip;
    lastSongStatus = "Melodia vuota";
    return;
  }

  startSong(count);

  lastSongIP = ip;
  lastSongStatus = "In riproduzione";

  String resp = "{";
  resp += "\"ok\":true,";
  resp += "\"notes\":" + String(count) + ",";
  resp += "\"gapMs\":" + String(songGapMs) + ",";
  resp += "\"duty\":" + String(buzzerDuty);
  resp += "}";

  server.send(200, "application/json", resp);
}

/* ===============================
   SETUP
   =============================== */

void setup()
{
  /*
     OUTPUT SERIALE DI AVVIO
     ----------------------
     Obiettivo:
     - rendere chiaro cosa sta facendo l'ESP32 durante il boot
     - facilitare il debug in laboratorio

     Nota:
     - Il bootloader ESP32 stampa a 115200 baud.
     - Manteniamo lo stesso baud rate per avere continuità.
  */

  Serial.begin(115200);
  delay(250);

  Serial.println();
  Serial.println("============================================================");
  Serial.println("RemSensESP32 - Avvio firmware");
  Serial.println("============================================================");

  /* Inizializzazione pin sensori */
  Serial.println("[BOOT] Inizializzazione pin sensori...");
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_TRIG, LOW);
  Serial.println("[BOOT] Pin HC-SR04: OK");

  /* ADC per LDR */
  Serial.println("[BOOT] Configurazione ADC (LDR)...");
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  Serial.println("[BOOT] ADC: risoluzione 12 bit, attenuazione 11dB (circa 0..3.3V)");

  /* DHT */
  Serial.println("[BOOT] Avvio sensore DHT11...");
  dht.begin();
  Serial.println("[BOOT] DHT11: OK (inizializzato)");

  /* PWM */
  Serial.println("[BOOT] Configurazione PWM (LED RGB + buzzer)...");
  initPwm();
  setLedRgb(0, 0, 0);
  Serial.println("[BOOT] PWM: OK");

  /* Test veloce attuatori */
  Serial.println("[BOOT] Self-test attuatori (LED + beep)...");
  setLedRgb(120, 0, 0);
  delay(150);
  setLedRgb(0, 120, 0);
  delay(150);
  setLedRgb(0, 0, 120);
  delay(150);
  setLedRgb(0, 0, 0);
  beepMs(80, 110);
  delay(80);
  beepMs(80, 110);
  Serial.println("[BOOT] Self-test attuatori: completato");

  /* WiFi */
  Serial.println("[BOOT] Avvio WiFi (multi-rete con IP statico)...");

  bool connected = false;

  for (int i = 0; i < wifiCount; i++) {
    Serial.println("------------------------------------------------------------");
    Serial.print("[WIFI] Tentativo ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(wifiCount);
    Serial.print(" -> SSID: ");
    Serial.println(wifiNetworks[i].ssid);

    bool okConfig = WiFi.config(
      wifiNetworks[i].local_IP,
      wifiNetworks[i].gateway,
      wifiNetworks[i].subnet,
      wifiNetworks[i].primaryDNS,
      wifiNetworks[i].secondaryDNS
    );

    if (okConfig) {
      Serial.print("[WIFI] IP statico configurato: ");
      Serial.println(wifiNetworks[i].local_IP.toString());
    } else {
      Serial.println("[WIFI] ATTENZIONE: configurazione IP statico fallita (si prosegue comunque)");
    }

    WiFi.begin(wifiNetworks[i].ssid, wifiNetworks[i].password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
      Serial.print(".");
      delay(500);
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      currentSSID = wifiNetworks[i].ssid;
      connected = true;
      Serial.println("[WIFI] Connesso!");
      Serial.print("[WIFI] SSID: ");
      Serial.println(currentSSID);
      Serial.print("[WIFI] IP ESP32: ");
      Serial.println(WiFi.localIP().toString());
      Serial.print("[WIFI] RSSI: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
      break;
    } else {
      Serial.println("[WIFI] Connessione fallita, si prova la rete successiva.");
      WiFi.disconnect(true);
      delay(200);
    }
  }

  if (!connected) {
    Serial.println("------------------------------------------------------------");
    Serial.println("[WIFI] ERRORE: nessuna rete disponibile. Il server web potrebbe non essere raggiungibile.");
    Serial.println("[WIFI] Suggerimenti: controllare SSID/password, copertura, canale WiFi, MAC filter.");
  }

  /* NTP */
  Serial.println("------------------------------------------------------------");
  Serial.println("[NTP] Configurazione orario (Italia, CET/CEST)...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  Serial.println("[NTP] Attesa sincronizzazione (max ~3s)...");
  unsigned long ntpStart = millis();
  String dt = getDateTime();

  while (dt == "NTP_ERROR" && millis() - ntpStart < 3000) {
    delay(250);
    dt = getDateTime();
    Serial.print(".");
  }
  Serial.println();

  if (dt == "NTP_ERROR") {
    Serial.println("[NTP] ATTENZIONE: sincronizzazione non disponibile al momento (si prosegue)");
  } else {
    Serial.print("[NTP] Orario sincronizzato: ");
    Serial.println(dt);
  }

  /* Server HTTP */
  Serial.println("------------------------------------------------------------");
  Serial.println("[HTTP] Registrazione endpoint e avvio server...");

  server.on("/", handleRoot);
  server.on("/log", handleLog);
  server.on("/sensors", handleSensors);
  server.on("/state", handleState);
  server.on("/setSafeDistance", handleSetSafeDistance);
  server.on("/setLightThreshold", handleSetLightThreshold);
  server.on("/setLed", handleSetLed);
  server.on("/beep", handleBeep);
  server.on("/playSong", handlePlaySong);
  server.on("/stopSong", handleStopSong);

  server.begin();

  Serial.println("[HTTP] Server pronto");

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[HTTP] URL: http://");
    Serial.print(WiFi.localIP().toString());
    Serial.println("/");
  } else {
    Serial.println("[HTTP] URL non stampabile (WiFi non connesso)");
  }

  Serial.println("============================================================");
  Serial.println("[BOOT] Avvio completato");
  Serial.println("============================================================");
}

/* ===============================
   LOOP
   =============================== */

void loop()
{
  /*
     Loop minimale e stabile:
     - gestisce le richieste HTTP
     - avanza il player musicale senza bloccare
  */

  server.handleClient();
  musicTick();
}

