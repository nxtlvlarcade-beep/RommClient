# ROMM Client v0.9

ROMM-Client-Projekt für klassische Amiga-Systeme und Retro-Computer.

Das Projekt unterstützt zwei Betriebsarten:

1. Proxy-/Gateway-Modus
2. Nativer Amiga-m68k-Modus

## 1. Proxy-/Gateway-Modus

Im Gateway-Modus läuft die ROMM-Anwendung auf einem Linux-Server
beziehungsweise in einem Docker-Container.

Der Amiga verbindet sich über einen Telnet-Client mit dem Gateway.

Das Gateway übernimmt:

- Verbindung zur ROMM-API
- Authentifizierung mit ROMM-API-Token
- Auflisten von Plattformen und Spielen
- Anzeigen von Spielinformationen
- Herunterladen von ROM-Dateien
- Optionale ZMODEM-Übertragung zum Amiga

### Einrichtung

Siehe:

    extras/romm-retro-gateway/README.md

Die Konfiguration erfolgt über eine .env-Datei.

Wichtige Variablen:

    ROMM_URL=YOUR_ROMM_URL
    ROMM_TOKEN=YOUR_ROMM_API_TOKEN
    ROMM_ZMODEM=1
    TELNET_PORT=2323

Der API-Token darf nicht in Git eingecheckt werden.

## 2. Nativer Amiga-m68k-Modus

Im nativen Modus läuft `romm-amiga` direkt unter AmigaOS.

Voraussetzungen:

- AmigaOS 3.x
- TCP/IP-Stack, beispielsweise Roadshow
- Netzwerkzugriff auf einen geeigneten HTTP-Proxy
- ROMM-Server mit API-Token

Der native Client benötigt kein Telnet-Gateway.

Er kommuniziert über den konfigurierten HTTP-Proxy mit ROMM.

### HTTP-Proxy

Der Proxy übernimmt die Verbindung zum ROMM-Server
und fügt den erforderlichen Authorization-Header hinzu.

Beispiel:

    Authorization: Bearer YOUR_ROMM_API_TOKEN

Der API-Token wird serverseitig konfiguriert und muss nicht
auf dem Amiga gespeichert werden.

Der Proxy muss ROMM-API-Anfragen und Downloads korrekt
weiterleiten.

### Start

Den für AmigaOS erzeugten m68k-Build auf den Amiga kopieren.

Den HTTP-Proxy entsprechend der verwendeten Client-Version
konfigurieren und `romm-amiga` starten.

Die genauen Startparameter hängen vom jeweiligen Build ab.

## 3. Bibliothek libromm

Die gemeinsame C-Bibliothek bildet die Grundlage für
die unterschiedlichen Clients.

Der Linux-TUI-Client befindet sich unter:

    examples/romm_tui.c

Die ZMODEM-Funktion ist optional und wird über
ROMM_ZMODEM aktiviert.

## 4. Repository-Struktur

    include/                     Öffentliche Header
    src/                         Bibliotheksquellen
    examples/                    Beispielprogramme und TUI
    extras/romm-retro-gateway/   Docker-/Telnet-Gateway
    extras/buildagent/           Build-Agent

## 5. Build-Agent

Der Build-Agent unterstützt die Erstellung der Projekt-Binaries.

Weitere Informationen:

    extras/buildagent/README.md

## 6. Sicherheit

- API-Tokens niemals in Git speichern.
- Echte .env-Dateien nicht veröffentlichen.
- HTTP-Proxies und Telnet nur in vertrauenswürdigen
  Netzwerken oder hinter geeigneten Schutzmechanismen betreiben.
- Für öffentlich erreichbare Dienste TLS und Zugriffsschutz verwenden.

## Version

0.9 – Integration des Retro-Gateways und des Build-Agents,
mit optionalem ZMODEM-Support.
