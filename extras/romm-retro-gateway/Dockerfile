
# Stage 1: ROMM-TUI kompilieren
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc make libc6-dev libcurl4-openssl-dev \
    pkg-config ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . ./
RUN make clean && make romm-tui romm-cli


# Stage 2: Gateway with Caddy and RetroWeb REST frontend
FROM python:3.12-slim-bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4 ca-certificates lrzsz caddy libarchive-tools \
    && rm -rf /var/lib/apt/lists/* \
    && useradd -r -u 10001 -d /downloads -s /usr/sbin/nologin retro

COPY --from=builder /build/romm-tui /usr/local/bin/romm-tui
COPY --from=builder /build/romm-cli /usr/local/bin/romm-cli
COPY extras/romm-retro-gateway/retro_web.py /usr/local/bin/retro_web.py
COPY extras/romm-retro-gateway/telnet_server.py /usr/local/bin/telnet_server.py
COPY extras/romm-retro-gateway/whdload_service.py /usr/local/bin/whdload_service.py
COPY extras/romm-retro-gateway/whdload_catalog.py /usr/local/bin/whdload_catalog.py
COPY extras/romm-retro-gateway/index_service.py /usr/local/bin/index_service.py
COPY extras/romm-retro-gateway/Caddyfile /etc/caddy/Caddyfile

RUN mkdir -p /downloads \
    && chown retro:retro /downloads

USER retro
WORKDIR /downloads

EXPOSE 80 2323 8080

# Caddy und Telnet gemeinsam starten
CMD ["sh", "-c", "python3 -u /usr/local/bin/whdload_service.py & python3 -u /usr/local/bin/index_service.py & python3 -u /usr/local/bin/retro_web.py & caddy run --config /etc/caddy/Caddyfile --adapter caddyfile & exec python3 -u /usr/local/bin/telnet_server.py"]
