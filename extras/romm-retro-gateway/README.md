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

    ROMM_URL=YOUR_ROM_URL
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


### v1.0-dev: Spieleindex

Der Gateway startet `index_service.py` auf `127.0.0.1:8091`.
`/api/roms?platform_ids=...` und `/api/roms/index-search` verwenden einen
plattformbezogenen RAM-Cache. Der erste Abruf lädt alle RomM-Seiten; danach
werden Seiten, Suchergebnisse und A-Z-Anfangsbuchstaben aus dem Cache bedient.
`ROMM_INDEX_TTL` (Sekunden, Standard 900) und `ROMM_INDEX_MAX_ROMS`
(Standard 100000 pro Plattform) steuern den Cache. Nach Ablauf wird beim
nächsten Zugriff neu geladen.

Die Amiga-Pfeiltasten links/rechts springen nun zu vorhandenen Anfangsbuchstaben
über die ganze Plattform. Ein echter Amiga-Cross-Build und der Live-Test gegen
RomM stehen noch aus.

**WHDLoad:** Die vorhandene Paketvorbereitung benötigt weiterhin genau einen
vorhandenen `.slave`. Eine universelle ADF/IPF-zu-WHDLoad-Konvertierung ist
nicht implementiert. Offizielle Installer können nicht ohne Prüfung ihrer
Version und Installationsanforderungen mit beliebigen ROM-Dateien kombiniert
werden.
