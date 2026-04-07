#ifndef WIFI_SECRETS_H
#define WIFI_SECRETS_H

#include <WiFi.h>

/*
  wifi_secrets.h
  --------------
  File separato per credenziali e parametri di rete.

  Suggerimento:
  - In un repository Git, aggiungere questo file al .gitignore.
  - In laboratorio, distribuire un template senza password reali.
*/

struct WifiConfig {
  const char* ssid;
  const char* password;
  IPAddress local_IP;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress primaryDNS;
  IPAddress secondaryDNS;
};

/* Elenco reti disponibili */
static WifiConfig wifiNetworks[] = {
  {
    "WifiSSID",
    "wifipassword",
    IPAddress(192,168,1,210),
    IPAddress(192,168,1,1),
    IPAddress(255,255,255,0),
    IPAddress(8,8,8,8),
    IPAddress(8,8,4,4)
  },
  {
    "LabSistemi",
    "password",
    IPAddress(172,16,4,35),
    IPAddress(172,16,0,254),
    IPAddress(255,255,0,0),
    IPAddress(8,8,8,8),
    IPAddress(8,8,4,4)
  }
};

/* Numero reti */
static const int wifiCount = sizeof(wifiNetworks) / sizeof(WifiConfig);

#endif
