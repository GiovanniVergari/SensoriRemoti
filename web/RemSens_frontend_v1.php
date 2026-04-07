<?php

/*
    ============================================================
    INDEX.PHP - VERSIONE UNIFICATA (FRONTEND + API)
    ============================================================

    Questo file gestisce:
    - Visualizzazione grafico + tabella
    - Endpoint API (?api=1) per restituire i dati in JSON

    NOTA:
    Le credenziali DB sono in db_config.php
*/

require_once 'db_config.php';


/*
    ============================================================
    MODALITÀ API (RISPOSTA JSON)
    ============================================================
*/

if (isset($_GET['api'])) {

    $conn = mysqli_connect($DB_HOST, $DB_USER, $DB_PASS, $DB_NAME);

    if ($conn === false) {
        http_response_code(500);
        header('Content-Type: application/json');
        echo json_encode(array("errore" => "Connessione fallita"));
        exit;
    }

    mysqli_set_charset($conn, "utf8mb4");

    $sql = "
        SELECT id, temperatura, umidita, luce, data_ora
        FROM sensori
        ORDER BY data_ora ASC
    ";

    $result = mysqli_query($conn, $sql);

    if ($result === false) {
        mysqli_close($conn);
        http_response_code(500);
        echo json_encode(array("errore" => "Errore query"));
        exit;
    }

    $data = array();

    while ($row = mysqli_fetch_assoc($result)) {
        $data[] = $row;
    }

    mysqli_free_result($result);
    mysqli_close($conn);

    header('Content-Type: application/json');
    echo json_encode($data, JSON_UNESCAPED_UNICODE);

    exit;
}

?>

<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="UTF-8">
    <title>Monitoraggio Sensori</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

    <style>

        body {
            font-family: Arial;
            background-color: #f4f6f8;
            margin: 0;
            padding: 20px;
        }

        .container {
            max-width: 1100px;
            margin: auto;
        }

        h1 {
            text-align: center;
        }

        .card {
            background: white;
            padding: 20px;
            margin-bottom: 20px;
            border-radius: 10px;
        }

        .chart-wrapper {
            height: 400px;
        }

        table {
            width: 100%;
            border-collapse: collapse;
        }

        th, td {
            border: 1px solid #ccc;
            padding: 8px;
            text-align: center;
        }

        th {
            background: #eee;
        }

        .status {
            text-align: center;
            margin-bottom: 15px;
        }

    </style>
</head>
<body>

<div class="container">

    <h1>Monitoraggio Sensori</h1>

    <div class="status">
        Ultimo aggiornamento: <span id="lastUpdate">--</span>
    </div>

    <!-- GRAFICO -->
    <div class="card">
        <h2>Grafico</h2>
        <div class="chart-wrapper">
            <canvas id="chart"></canvas>
        </div>
    </div>

    <!-- TABELLA -->
    <div class="card">
        <h2>Tabella dati</h2>

        <table>
            <thead>
                <tr>
                    <th>ID</th>
                    <th>Temp</th>
                    <th>Umidità</th>
                    <th>Luce</th>
                    <th>Data</th>
                </tr>
            </thead>
            <tbody id="tbody">
                <tr><td colspan="5">Caricamento...</td></tr>
            </tbody>
        </table>
    </div>

</div>

<script>

/*
    ============================================================
    VARIABILI GLOBALI
    ============================================================
*/

let chart = null;


/*
    ============================================================
    CREAZIONE GRAFICO
    ============================================================
*/

function createChart(labels, t, h, l) {

    const ctx = document.getElementById("chart").getContext("2d");

    chart = new Chart(ctx, {
        type: "line",
        data: {
            labels: labels,
            datasets: [
                {
                    label: "Temperatura",
                    data: t,
                    borderWidth: 2
                },
                {
                    label: "Umidità",
                    data: h,
                    borderWidth: 2
                },
                {
                    label: "Luce",
                    data: l,
                    borderWidth: 2
                }
            ]
        }
    });
}


/*
    ============================================================
    AGGIORNAMENTO GRAFICO
    ============================================================
*/

function updateChart(labels, t, h, l) {

    chart.data.labels = labels;

    chart.data.datasets[0].data = t;
    chart.data.datasets[1].data = h;
    chart.data.datasets[2].data = l;

    chart.update();
}


/*
    ============================================================
    AGGIORNAMENTO TABELLA
    ============================================================
*/

function updateTable(data) {

    const tbody = document.getElementById("tbody");
    tbody.innerHTML = "";

    for (let i = 0; i < data.length; i++) {

        const r = data[i];

        const tr = document.createElement("tr");

        tr.innerHTML =
            "<td>" + r.id + "</td>" +
            "<td>" + r.temperatura + "</td>" +
            "<td>" + r.umidita + "</td>" +
            "<td>" + r.luce + "</td>" +
            "<td>" + r.data_ora + "</td>";

        tbody.appendChild(tr);
    }
}


/*
    ============================================================
    FETCH DATI
    ============================================================
*/

function fetchData() {

    fetch("index.php?api=1")
        .then(function(res) { return res.json(); })
        .then(function(data) {

            const labels = [];
            const t = [];
            const h = [];
            const l = [];

            for (let i = 0; i < data.length; i++) {

                labels.push(data[i].data_ora);
                t.push(parseFloat(data[i].temperatura));
                h.push(parseFloat(data[i].umidita));
                l.push(parseFloat(data[i].luce));
            }

            if (chart === null) {
                createChart(labels, t, h, l);
            } else {
                updateChart(labels, t, h, l);
            }

            updateTable(data);

            const now = new Date();
            document.getElementById("lastUpdate").textContent =
                now.toLocaleTimeString();
        });
}


/*
    ============================================================
    AVVIO
    ============================================================
*/

fetchData();
setInterval(fetchData, 5000);

</script>

</body>
</html>