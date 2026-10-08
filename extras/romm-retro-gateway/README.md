# ROMM Retro Gateway

Docker-basierter ROMM-Client für klassische Computer mit Telnet-Unterstützung.

## Funktionsweise

Der Amiga verbindet sich per DCTelnet mit dem Gateway.

Das Gateway startet `romm-tui`, verbindet sich mit der ROMM-API
und ermöglicht die Auswahl und den Download von Spielen.

Optional werden Dateien per ZMODEM zum Amiga übertragen.

## Voraussetzungen

- Docker
- ROMM-Server
- ROMM-API-Token
- Telnet-Client auf dem Amiga
- Für ZMODEM: kompatibler ZMODEM-Empfänger, z. B. DCTelnet mit XPR

## Konfiguration

Beispielkonfiguration erstellen:

    cp extras/romm-retro-gateway/.env.example \
       extras/romm-retro-gateway/.env

In der .env-Datei konfigurieren:

    ROMM_URL=https://play.next-level.fun
    ROMM_TOKEN=YOUR_ROMM_API_TOKEN
    ROMM_ZMODEM=1
    TELNET_PORT=2323

ROMM_URL:
Adresse des ROMM-Servers.

ROMM_TOKEN:
API-Token für den ROMM-Zugriff.

ROMM_ZMODEM:
1 aktiviert ZMODEM-Dateiübertragungen.
0 deaktiviert ZMODEM und verwendet den normalen Downloadmodus.

TELNET_PORT:
Port des Telnet-Gateways.

## Docker-Build

Vom Hauptverzeichnis des Repositorys:

    docker build \
      -f extras/romm-retro-gateway/Dockerfile \
      -t romm-retro-gateway:v0.9 .

## Betrieb

Der Container benötigt Zugriff auf die ROMM-API.

Der Telnet-Port muss für den Amiga erreichbar sein.

Das Gateway verwendet `sz` aus dem Paket `lrzsz` für ZMODEM.

Hinweis: Telnet ist unverschlüsselt. Das Gateway sollte ausschließlich
in einem vertrauenswürdigen Netzwerk oder über eine gesicherte
Verbindung erreichbar sein.
