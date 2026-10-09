# libromm 1.1

libromm is a C client project for browsing a RomM library from AmigaOS 3.x and terminal environments. Version 1.1 adds a browser-based RetroWeb frontend and first-run configuration for the Docker gateway.

## Components

- Native AmigaOS 3.x client (`examples/romm_amiga.c`).
- Terminal/Telnet TUI (`examples/romm_tui.c`).
- Retro gateway with RomM REST API proxy, RAW and WHDLoad download services.
- RetroWeb: simple HTTP/1.0-compatible HTML, game search, covers and screenshots, without JavaScript.
- Browser-based configuration at `/config` (RomM URL/token, Telnet port, ZMODEM).
- Jenkins build agent (`extras/buildagent/`).

## Build

From the repository root:

```sh
make clean && make -j2
make -f Makefile.amiga clean && make -f Makefile.amiga -j2
# Gateway image
docker build -f extras/romm-retro-gateway/Dockerfile -t romm-retro-gateway:1.1-rest .
```

The Amiga build requires the configured m68k/AmigaOS cross-compiler.

## Gateway installation

See **[Gateway installation and first-run configuration](extras/romm-retro-gateway/README.md)**. No pre-created `.env` is required for a fresh Docker installation. Mount `/downloads` persistently; retrieve the setup password from Docker logs and visit `http://GATEWAY-IP/config`.

## Security

Use only on a trusted LAN. `/config` uses HTTP Basic authentication, and both HTTP and Telnet are unencrypted. Do not expose the gateway to the public internet. Keep the persistent configuration and setup password private; do not commit API tokens.

## Release status

Stable release: **v1.1**. Includes AmigaOS 3.x applications, CLI, TUI, RetroWeb, Telnet/ZMODEM and web-based first-run configuration.
