
# Stage 1: ROMM-TUI kompilieren
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc make libc6-dev libcurl4-openssl-dev \
    pkg-config ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY libromm/ ./
RUN make clean && make romm-tui


# Stage 2: Gateway mit Caddy und ZMODEM
FROM python:3.12-slim-bookworm

RUN apt-get update && apt-get install -y --no-install-recommends \
    libcurl4 ca-certificates lrzsz caddy \
    && rm -rf /var/lib/apt/lists/* \
    && useradd -r -u 10001 -d /downloads -s /usr/sbin/nologin retro

COPY --from=builder /build/romm-tui /usr/local/bin/romm-tui
COPY telnet_server.py /usr/local/bin/telnet_server.py
COPY Caddyfile /etc/caddy/Caddyfile

RUN mkdir -p /downloads \
    && chown retro:retro /downloads

USER retro
WORKDIR /downloads

EXPOSE 2323 8080

# Caddy und Telnet gemeinsam starten
CMD ["sh", "-c", "caddy run --config /etc/caddy/Caddyfile --adapter caddyfile & exec python3 -u /usr/local/bin/telnet_server.py"]
