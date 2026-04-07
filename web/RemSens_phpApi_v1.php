<?php

/*
    ============================================================
    API PHP - Versione studenti
    File: api_sensori.php
    ============================================================

    SCOPO
    -----
    Questo script:
    1) riceve una richiesta POST
    2) legge il body JSON della richiesta
    3) estrae i valori:
         - temperatura
         - umidita
         - luce
    4) inserisce i dati nel database
    5) restituisce una risposta JSON

    FORMATO JSON ATTESO
    -------------------
    {
        "temperatura": 23.5,
        "umidita": 60,
        "luce": 72
    }

    NOTA
    ----
    Le credenziali del database sono salvate
    in un file separato: db_config.php
*/


/*
    ============================================================
    HEADER DELLA RISPOSTA
    ============================================================
*/

header('Content-Type: application/json; charset=utf-8');


/*
    ============================================================
    INCLUSIONE CONFIGURAZIONE DATABASE
    ============================================================
*/

require_once 'db_config.php';


/*
    ============================================================
    FUNZIONE DI SUPPORTO
    ============================================================

    Questa funzione serve per restituire una risposta JSON
    sempre con la stessa struttura.
*/

function send_json_response($status_code, $success, $message, $extra_data = array())
{
    http_response_code($status_code);

    $response = array();
    $response['success'] = $success;
    $response['message'] = $message;

    foreach ($extra_data as $key => $value) {
        $response[$key] = $value;
    }

    echo json_encode($response, JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
    exit;
}


/*
    ============================================================
    1) CONTROLLO DEL METODO HTTP
    ============================================================

    Questa API accetta solo richieste POST.
    Se arriva una GET o un altro metodo, rispondiamo con errore.
*/

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    send_json_response(
        405,
        false,
        'Metodo non consentito. Utilizzare POST.'
    );
}


/*
    ============================================================
    2) LETTURA DEL BODY DELLA RICHIESTA
    ============================================================

    Il JSON inviato dal client Python non arriva in $_POST,
    ma nel body raw della richiesta HTTP.

    Per questo usiamo:
        file_get_contents("php://input")
*/

$raw_input = file_get_contents('php://input');

if ($raw_input === false || trim($raw_input) === '') {
    send_json_response(
        400,
        false,
        'Body della richiesta vuoto.'
    );
}


/*
    ============================================================
    3) DECODIFICA DEL JSON
    ============================================================

    json_decode(..., true) converte il JSON in un array associativo PHP.
*/

$data = json_decode($raw_input, true);

if ($data === null && json_last_error() !== JSON_ERROR_NONE) {
    send_json_response(
        400,
        false,
        'JSON non valido.',
        array(
            'json_error' => json_last_error_msg()
        )
    );
}


/*
    ============================================================
    4) ESTRAZIONE DEI CAMPI
    ============================================================

    Cerchiamo i tre campi richiesti dalla traccia:
    - temperatura
    - umidita
    - luce
*/

$temperatura = null;
$umidita = null;
$luce = null;

if (array_key_exists('temperatura', $data)) {
    $temperatura = $data['temperatura'];
}

if (array_key_exists('umidita', $data)) {
    $umidita = $data['umidita'];
}

if (array_key_exists('luce', $data)) {
    $luce = $data['luce'];
}


/*
    ============================================================
    5) VALIDAZIONE DEI DATI
    ============================================================

    Controlliamo:
    - che i campi esistano
    - che siano numerici
*/

if ($temperatura === null || $umidita === null || $luce === null) {
    send_json_response(
        400,
        false,
        'Campi mancanti. Sono richiesti: temperatura, umidita, luce.'
    );
}

if (!is_numeric($temperatura)) {
    send_json_response(
        400,
        false,
        'Il campo temperatura deve essere numerico.'
    );
}

if (!is_numeric($umidita)) {
    send_json_response(
        400,
        false,
        'Il campo umidita deve essere numerico.'
    );
}

if (!is_numeric($luce)) {
    send_json_response(
        400,
        false,
        'Il campo luce deve essere numerico.'
    );
}


/*
    ============================================================
    6) CONVERSIONE DEI TIPI
    ============================================================

    Convertiamo esplicitamente i valori in float.
    Questo rende più chiaro il tipo di dato con cui lavoriamo.
*/

$temperatura = floatval($temperatura);
$umidita = floatval($umidita);
$luce = floatval($luce);


/*
    ============================================================
    7) CONNESSIONE AL DATABASE
    ============================================================
*/

$conn = mysqli_connect($DB_HOST, $DB_USER, $DB_PASS, $DB_NAME);

if ($conn === false) {
    send_json_response(
        500,
        false,
        'Connessione al database fallita.'
    );
}

mysqli_set_charset($conn, 'utf8mb4');


/*
    ============================================================
    8) PREPARAZIONE DELLA QUERY SQL
    ============================================================

    Usiamo una prepared statement per maggiore sicurezza.
*/

$sql = "
    INSERT INTO sensori (
        temperatura,
        umidita,
        luce
    )
    VALUES (
        ?,
        ?,
        ?
    )
";

$stmt = mysqli_prepare($conn, $sql);

if ($stmt === false) {
    mysqli_close($conn);

    send_json_response(
        500,
        false,
        'Preparazione della query fallita.'
    );
}


/*
    ============================================================
    9) ASSOCIAZIONE DEI PARAMETRI
    ============================================================

    'ddd' significa:
    - d = double
    - d = double
    - d = double
*/

$bind_ok = mysqli_stmt_bind_param(
    $stmt,
    'ddd',
    $temperatura,
    $umidita,
    $luce
);

if ($bind_ok === false) {
    mysqli_stmt_close($stmt);
    mysqli_close($conn);

    send_json_response(
        500,
        false,
        'Associazione dei parametri fallita.'
    );
}


/*
    ============================================================
    10) ESECUZIONE DELLA QUERY
    ============================================================
*/

$execute_ok = mysqli_stmt_execute($stmt);

if ($execute_ok === false) {
    $db_error = mysqli_stmt_error($stmt);

    mysqli_stmt_close($stmt);
    mysqli_close($conn);

    send_json_response(
        500,
        false,
        'Inserimento nel database fallito.',
        array(
            'db_error' => $db_error
        )
    );
}


/*
    ============================================================
    11) RECUPERO ID DEL RECORD INSERITO
    ============================================================
*/

$insert_id = mysqli_insert_id($conn);


/*
    ============================================================
    12) CHIUSURA RISORSE
    ============================================================
*/

mysqli_stmt_close($stmt);
mysqli_close($conn);


/*
    ============================================================
    13) RISPOSTA FINALE
    ============================================================
*/

send_json_response(
    200,
    true,
    'Dati inseriti correttamente.',
    array(
        'insert_id' => $insert_id,
        'temperatura' => $temperatura,
        'umidita' => $umidita,
        'luce' => $luce
    )
);