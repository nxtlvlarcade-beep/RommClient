# libromm Build-Agent

Docker-basierter Jenkins Inbound-Agent zum Kompilieren von libromm
und nativen AmigaOS-3.x-Anwendungen.

## Komponenten

- Jenkins Inbound-Agent (Java 21)
- GCC / G++ / Make für Linux
- Git und Build-Werkzeuge
- libcurl-Entwicklungsbibliotheken
- AmigaOS-m68k-Cross-Compiler

Der Cross-Compiler wird aus folgendem Projekt erstellt:

https://github.com/AmigaPorts/m68k-amigaos-gcc

Installationsverzeichnis im Container:

    /opt/amiga

Der Compiler-Pfad wird automatisch zu PATH hinzugefügt.

## Docker-Image erstellen

Vom Hauptverzeichnis des Repositorys:

    docker build \
      -f extras/buildagent/Dockerfile \
      -t libromm-build-agent:v0.9 \
      extras/buildagent

Hinweis: Die Erstellung des Cross-Compilers kann längere Zeit dauern.

## Jenkins-Konfiguration

In Jenkins einen Agenten mit folgenden Eigenschaften erstellen:

- Typ: Permanent Agent
- Launch-Methode: Inbound Agent
- Remote Root Directory: /home/jenkins/agent

Den Agent-Namen und die Verbindungseinstellungen entsprechend
der Jenkins-Installation konfigurieren.

## Container starten

Beispiel für einen Jenkins-Controller mit WebSocket-Unterstützung:

    docker run -d \
      --name libromm-build-agent \
      --restart unless-stopped \
      -e JENKINS_URL=https://jenkins.example.org/ \
      -e JENKINS_AGENT_NAME=libromm-build-agent \
      -e JENKINS_SECRET=YOUR_AGENT_SECRET \
      libromm-build-agent:v0.9

JENKINS_URL:
Adresse des Jenkins-Controllers.

JENKINS_AGENT_NAME:
Name des in Jenkins konfigurierten Agents.

JENKINS_SECRET:
Verbindungs-Secret des Jenkins-Agents.

Das Secret niemals in Git speichern.

Für Jenkins-Verbindungen über WebSocket muss der Agent
mit der entsprechenden WebSocket-Option gestartet werden.
Die genaue Startkonfiguration hängt vom Jenkins-Controller ab.

## Builds

Der Agent kann Linux-Binaries und AmigaOS-m68k-Binaries
erstellen, sofern die jeweiligen Makefiles und Abhängigkeiten
für die Zielplattform eingerichtet sind.

## Sicherheit

- Jenkins-Secrets nicht ins Repository aufnehmen.
- Agent-Zugriff auf vertrauenswürdige Jenkins-Controller begrenzen.
- Container-Images regelmäßig aktualisieren.
