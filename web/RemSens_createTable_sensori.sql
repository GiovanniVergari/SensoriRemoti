/*
    ============================================================
    CREAZIONE TABELLA sensori
    Versione studenti
    ============================================================

    ATTENZIONE
    ----------
    - Il database è già stato creato.
    - Assicurarsi di aver selezionato il database corretto
      prima di eseguire questo script.

    Esempio:
        USE nome_database;

    SCOPO
    -----
    Creare una tabella per salvare:
    - temperatura
    - umidita
    - luce

    Ogni riga rappresenta una rilevazione.
*/


/*
    ============================================================
    CREAZIONE TABELLA
    ============================================================
*/

CREATE TABLE IF NOT EXISTS sensori (

    /*
        ID univoco del record
        AUTO_INCREMENT: incremento automatico
        PRIMARY KEY: chiave primaria
    */
    id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,

    /*
        Temperatura (°C)
    */
    temperatura FLOAT NOT NULL,

    /*
        Umidità (%)
    */
    umidita FLOAT NOT NULL,

    /*
        Luce (%)
    */
    luce FLOAT NOT NULL,

    /*
        Data e ora della rilevazione
        Inserita automaticamente dal database
    */
    data_ora TIMESTAMP DEFAULT CURRENT_TIMESTAMP

);