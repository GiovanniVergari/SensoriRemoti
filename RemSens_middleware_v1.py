"""
microservice_forwarder_studenti.py

OBIETTIVO
---------
Questo script:
1) interroga l'ESP32 tramite l'endpoint /state
2) riceve un JSON con i dati dei sensori
3) salva localmente il JSON ricevuto
4) estrae solo i campi utili alla traccia:
      - temperatura
      - umidita
      - luce
5) invia questi dati a un server remoto tramite POST

REQUISITI
---------
Installare la libreria requests con:

    pip install requests
"""

import json
import time
from pathlib import Path
from datetime import datetime

import requests


# ============================================================
# CONFIGURAZIONE
# ============================================================

# Indirizzo dell'ESP32 nella rete locale
ESP32_BASE_URL = "http://192.168.1.210"

# Endpoint che restituisce lo stato completo del dispositivo
ESP32_STATE_ENDPOINT = "/state"

# URL remoto della API PHP che riceverà i dati
REMOTE_POST_URL = "https://example.com/api_sensori.php"

# Intervallo di tempo tra una lettura e la successiva
POLL_INTERVAL_SECONDS = 10

# Timeout massimo per ogni richiesta HTTP
HTTP_TIMEOUT_SECONDS = 5

# Cartella in cui salvare i file JSON ricevuti
ARCHIVE_DIR = Path("json_archive")

# File che conterrà sempre l'ultima lettura ricevuta
LATEST_JSON_FILE = Path("latest_state.json")


# ============================================================
# FUNZIONI DI SUPPORTO
# ============================================================

def print_separator():
    """
    Stampa una linea di separazione nel terminale
    per rendere il log più leggibile.
    """
    print("============================================================")


def get_now_string():
    """
    Restituisce data e ora correnti come stringa.
    """
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def ensure_archive_dir():
    """
    Crea la cartella di archivio se non esiste.
    """
    ARCHIVE_DIR.mkdir(parents=True, exist_ok=True)


def fetch_esp32_state():
    """
    Effettua una richiesta GET verso l'ESP32
    e restituisce il JSON ricevuto come dizionario Python.
    """
    url = ESP32_BASE_URL + ESP32_STATE_ENDPOINT

    print_separator()
    print("[INFO] Richiesta GET verso ESP32")
    print("[INFO] URL:", url)

    response = requests.get(url, timeout=HTTP_TIMEOUT_SECONDS)

    print("[INFO] Status code ESP32:", response.status_code)

    # Se il server risponde con errore HTTP, viene sollevata un'eccezione
    response.raise_for_status()

    # Converte il testo JSON in un dizionario Python
    data = response.json()

    print("[INFO] JSON ricevuto correttamente dall'ESP32")
    print(json.dumps(data, indent=2, ensure_ascii=False))

    return data


def save_raw_json_locally(data):
    """
    Salva il JSON originale ricevuto dall'ESP32 in due modi:
    1) file storico con timestamp
    2) file fisso con ultima lettura
    """
    ensure_archive_dir()

    timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    archive_file = ARCHIVE_DIR / ("state_" + timestamp + ".json")

    with archive_file.open("w", encoding="utf-8") as file_handle:
        json.dump(data, file_handle, indent=2, ensure_ascii=False)

    with LATEST_JSON_FILE.open("w", encoding="utf-8") as file_handle:
        json.dump(data, file_handle, indent=2, ensure_ascii=False)

    print("[INFO] JSON salvato in archivio:", archive_file)
    print("[INFO] Ultimo JSON salvato in:", LATEST_JSON_FILE)

    return archive_file


def safe_get_dict(parent_dict, key_name):
    """
    Restituisce un sotto-dizionario se presente.
    Se il campo non esiste o non è un dizionario,
    restituisce un dizionario vuoto.
    """
    if key_name in parent_dict and isinstance(parent_dict[key_name], dict):
        return parent_dict[key_name]

    return {}


def safe_get_number(parent_dict, key_name):
    """
    Restituisce un numero se il campo esiste ed è int o float.
    Altrimenti restituisce None.
    """
    if key_name not in parent_dict:
        return None

    value = parent_dict[key_name]

    if isinstance(value, int):
        return value

    if isinstance(value, float):
        return value

    return None


def build_remote_payload(raw_state):
    """
    Costruisce il payload ridotto da inviare al server remoto.

    Anche se il JSON dell'ESP32 contiene molti dati,
    la traccia richiede di inviare soltanto:
    - temperatura
    - umidita
    - luce

    Lo script cerca prima dentro raw_state["sensors"].
    Se non trova il dato, prova nei campi principali.
    """
    print_separator()
    print("[INFO] Costruzione payload remoto")

    sensors = safe_get_dict(raw_state, "sensors")

    # ========================================================
    # TEMPERATURA
    # ========================================================
    temperatura = safe_get_number(sensors, "temperature")

    if temperatura is None:
        temperatura = safe_get_number(sensors, "temperatureC")

    if temperatura is None:
        temperatura = safe_get_number(raw_state, "temperature")

    if temperatura is None:
        temperatura = safe_get_number(raw_state, "temperatureC")

    # ========================================================
    # UMIDITA'
    # ========================================================
    umidita = safe_get_number(sensors, "humidity")

    if umidita is None:
        umidita = safe_get_number(sensors, "humidityPct")

    if umidita is None:
        umidita = safe_get_number(raw_state, "humidity")

    if umidita is None:
        umidita = safe_get_number(raw_state, "humidityPct")

    # ========================================================
    # LUCE
    # ========================================================
    luce = safe_get_number(sensors, "ldrPercent")

    if luce is None:
        luce = safe_get_number(raw_state, "ldrPercent")

    if luce is None:
        luce = safe_get_number(raw_state, "light")

    # ========================================================
    # PAYLOAD FINALE
    # ========================================================
    payload = {
        "temperatura": temperatura,
        "umidita": umidita,
        "luce": luce
    }

    print("[INFO] Payload costruito:")
    print(json.dumps(payload, indent=2, ensure_ascii=False))

    return payload


def post_to_remote_server(payload):
    """
    Invia il payload al server remoto tramite POST JSON.
    """
    print_separator()
    print("[INFO] Invio POST al server remoto")
    print("[INFO] URL remoto:", REMOTE_POST_URL)

    headers = {
        "Content-Type": "application/json"
    }

    body = json.dumps(payload, ensure_ascii=False)

    print("[INFO] Header inviati:")
    print(headers)

    print("[INFO] Body inviato:")
    print(body)

    response = requests.post(
        REMOTE_POST_URL,
        headers=headers,
        data=body,
        timeout=HTTP_TIMEOUT_SECONDS
    )

    print("[INFO] Status code risposta remota:", response.status_code)

    response.raise_for_status()

    print("[INFO] Risposta server remoto:")
    print(response.text)

    return response


def print_cycle_report(cycle_number, archive_file, payload, remote_response):
    """
    Stampa un riepilogo finale del ciclo di acquisizione.
    """
    print_separator()
    print("[REPORT] Ciclo numero:", cycle_number)
    print("[REPORT] Orario:", get_now_string())
    print("[REPORT] File archivio:", archive_file)
    print("[REPORT] Temperatura:", payload["temperatura"])
    print("[REPORT] Umidita:", payload["umidita"])
    print("[REPORT] Luce:", payload["luce"])
    print("[REPORT] POST remoto:", remote_response.status_code)
    print_separator()


# ============================================================
# CICLO PRINCIPALE
# ============================================================

def run_forwarder():
    """
    Esegue il ciclo continuo del microservizio.

    Ad ogni iterazione:
    1) legge /state dall'ESP32
    2) salva il JSON completo
    3) costruisce il payload ridotto
    4) lo invia al server remoto
    """
    cycle_number = 0

    print_separator()
    print("[INFO] Avvio microservizio studenti")
    print("[INFO] Ora avvio:", get_now_string())
    print("[INFO] Endpoint ESP32:", ESP32_BASE_URL + ESP32_STATE_ENDPOINT)
    print("[INFO] Endpoint remoto:", REMOTE_POST_URL)
    print("[INFO] Intervallo (s):", POLL_INTERVAL_SECONDS)
    print_separator()

    while True:
        cycle_number = cycle_number + 1

        try:
            print_separator()
            print("[INFO] Inizio ciclo:", cycle_number)

            raw_state = fetch_esp32_state()
            archive_file = save_raw_json_locally(raw_state)
            payload = build_remote_payload(raw_state)
            remote_response = post_to_remote_server(payload)

            print_cycle_report(cycle_number, archive_file, payload, remote_response)

        except requests.RequestException as network_error:
            print_separator()
            print("[ERRORE DI RETE]")
            print(type(network_error).__name__)
            print(network_error)
            print_separator()

        except json.JSONDecodeError as json_error:
            print_separator()
            print("[ERRORE JSON]")
            print(type(json_error).__name__)
            print(json_error)
            print_separator()

        except Exception as generic_error:
            print_separator()
            print("[ERRORE GENERICO]")
            print(type(generic_error).__name__)
            print(generic_error)
            print_separator()

        print("[INFO] Attesa di", POLL_INTERVAL_SECONDS, "secondi...")
        time.sleep(POLL_INTERVAL_SECONDS)


# ============================================================
# MAIN
# ============================================================

if __name__ == "__main__":
    run_forwarder()