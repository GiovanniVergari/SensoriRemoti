# SensoriRemoti

Progetto didattico per la raccolta, l'inoltro, la memorizzazione e la visualizzazione di dati provenienti da un dispositivo ESP32.

## Obiettivo del progetto

Il sistema realizza una pipeline completa:

1. **ESP32**: espone un endpoint `/state` che restituisce un JSON con i dati dei sensori.
2. **Python**: interroga periodicamente l'ESP32, salva localmente il JSON ricevuto ed estrae solo i campi richiesti dalla traccia.
3. **PHP API**: riceve i dati via `POST` in formato JSON e li inserisce nel database.
4. **MySQL**: conserva le rilevazioni nella tabella `sensori`.
5. **Frontend PHP**: mostra grafico e tabella con aggiornamento automatico ogni 5 secondi.

---

## Struttura dei file

- `RemSens_middleware_v1.py`  
  Microservizio Python che legge il JSON dell'ESP32 e inoltra al server remoto.

- `RemSens_phpApi_v1.php`  
  API PHP che riceve il JSON e salva i dati nel database.

- `db_config.php`  
  File separato con i parametri di connessione al database.

- `RemSens_createTable_sensori.sql`  
  Script SQL per creare la tabella `sensori`.

- `RemSens_frontend_v1.php`  
  Pagina PHP che mostra grafico e tabella dei dati raccolti.

---

## Flusso dei dati

```text
ESP32 (/state) → Python → PHP API → MySQL → Frontend PHP
```

### Dati letti dall'ESP32
Il middleware Python legge il JSON completo da `/state`, ma invia al server remoto solo i campi richiesti dalla traccia:

- `temperatura`
- `umidita`
- `luce`

### Payload inviato al server remoto

```json
{
  "temperatura": 23.5,
  "umidita": 60,
  "luce": 72
}
```

---

## Configurazione database

Nel file `db_config.php` inserire i dati reali del database:

```php
<?php
$DB_HOST = 'localhost';
$DB_USER = 'nome_utente_db';
$DB_PASS = 'password_db';
$DB_NAME = 'nome_database';
```

---

## Creazione tabella

Lo script SQL non crea il database: assume che il database esista già e che sia già stato selezionato.

Eseguire quindi:

```sql
CREATE TABLE IF NOT EXISTS sensori (
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    temperatura FLOAT NOT NULL,
    umidita FLOAT NOT NULL,
    luce FLOAT NOT NULL,
    data_ora TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

---

## Uso del middleware Python

### Requisiti

Installare la libreria `requests`:

```bash
pip install requests
```

### Parametri da configurare
Nel file `RemSens_middleware_v1.py` modificare almeno:

```python
ESP32_BASE_URL = "http://192.168.1.210"
REMOTE_POST_URL = "https://example.com/api_sensori.php"
```

### Avvio

```bash
python RemSens_middleware_v1.py
```

Il programma:
- legge l'endpoint `/state`
- salva il JSON in archivio locale
- costruisce il payload ridotto
- invia i dati alla API PHP remota

---

## Uso della API PHP

Il file `RemSens_phpApi_v1.php` accetta **solo richieste POST** con `Content-Type: application/json`.

### Esempio di richiesta

```http
POST /api_sensori.php HTTP/1.1
Content-Type: application/json

{
  "temperatura": 23.5,
  "umidita": 60,
  "luce": 72
}
```

### Risposta prevista

```json
{
  "success": true,
  "message": "Dati inseriti correttamente.",
  "insert_id": 1,
  "temperatura": 23.5,
  "umidita": 60,
  "luce": 72
}
```

---

## Uso del frontend PHP

Il file `RemSens_frontend_v1.php` contiene sia:

- la pagina HTML con grafico e tabella
- una modalità API interna attivata con `?api=1`

### Importante
Il file, così com'è, usa nel JavaScript:

```javascript
fetch("index.php?api=1")
```

Quindi ci sono due possibilità:

1. **Rinominare il file in `index.php`**  
   Questa è la soluzione più semplice.

2. **Modificare la fetch** in modo coerente con il nome reale del file  
   Ad esempio, se il file si chiama `RemSens_frontend_v1.php`, allora usare:

```javascript
fetch("RemSens_frontend_v1.php?api=1")
```

---

## Verifica di coerenza tra i file

### Coerenze corrette

- Il middleware Python invia i campi `temperatura`, `umidita` e `luce`.
- La API PHP si aspetta esattamente `temperatura`, `umidita` e `luce`.
- La tabella SQL contiene i campi `temperatura`, `umidita`, `luce`, `data_ora`.
- Il frontend legge proprio questi stessi campi dalla tabella `sensori`.

### Punti da correggere / verificare

1. **URL remoto nel middleware Python**  
   Attualmente è un placeholder:

   ```python
   REMOTE_POST_URL = "https://example.com/api_sensori.php"
   ```

   Deve essere sostituito con l'URL reale della tua API.

2. **Nome del file frontend**  
   Il frontend attualmente usa `fetch("index.php?api=1")`, ma il file caricato si chiama `RemSens_frontend_v1.php`.
   Occorre:
   - o rinominare il file in `index.php`
   - o cambiare la `fetch`

3. **db_config.php**  
   Contiene ancora credenziali segnaposto e deve essere completato con i dati reali.

---

## Possibili estensioni future

- aggiunta di altri sensori al payload
- filtri temporali sul frontend
- esportazione CSV
- dashboard con più grafici
- autenticazione per l'API

---

## Licenza d'uso

Materiale didattico destinato ad attività di laboratorio scolastico.
