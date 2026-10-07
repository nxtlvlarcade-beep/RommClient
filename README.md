# libromm 0.5

Portable C99 ROMM client core plus libcurl transport and two frontends.

## New in 0.5

- `romm_game_t` detail metadata: summary, genres, developers, publishers, modes,
  regions, file size, release timestamp, rating, manual/multi-file flags and cover paths.
- `romm-cli info ROM_ID` prints the game description and metadata.
- `romm-tui`: ANSI/POSIX interactive two-column frontend.
  - Up/Down: selection
  - Enter on platform: open games
  - Enter on game: download using original ROM filename
  - Enter again when downloaded: launch via `ROMM_LAUNCHER`
  - Esc: games -> platforms
  - Q: quit
- Games are fetched in pages of 100 rather than loading a whole platform at once.

The TUI is deliberately outside `libromm`: the core remains GUI/terminal independent
for future AmigaOS 3.x, MorphOS and other frontends.

## Build

    make clean && make

## CLI

    ./romm-cli BASE_URL TOKEN info 81444

## TUI

    ./romm-tui BASE_URL TOKEN

Optional launcher example on Linux:

    export ROMM_LAUNCHER=/usr/bin/xdg-open
    ./romm-tui BASE_URL TOKEN

For an emulator, point `ROMM_LAUNCHER` at a wrapper executable/script that accepts the
downloaded ROM path as its first argument. This launcher hook is frontend-specific and
will later be replaced by an Amiga/MorphOS launcher backend.

## 0.7: AmigaOS 3.x / 68020 experimental port

This release adds an experimental native AmigaOS 3.x frontend (`romm-amiga`) and an
HTTP-only `bsdsocket.library` transport. The portable libromm core remains shared with
the host build.

### Cross build

A GCC AmigaOS cross toolchain that provides `m68k-amigaos-gcc`, NDK headers and
bsdsocket/AmiTCP-compatible headers is required:

    make -f Makefile.amiga clean
    make -f Makefile.amiga

Default CPU target is 68020 (`-m68020`), suitable as a baseline for an accelerated
A1200. Override `CFLAGS` if required.

### Network model

`romm-amiga` deliberately supports `http://` only. Use it only on a trusted LAN and
place a small HTTP reverse proxy in front of the HTTPS ROMM server. The proxy must
forward `/api/...` and download requests to ROMM. Do not expose that plaintext proxy
to an untrusted network.

    romm-amiga http://192.168.0.20:8088 TOKEN

The bearer token is still supplied on the command line in this experimental release.
A later revision should read it from a config/environment source so it is not exposed
in command history/process arguments.

### Controls

- Up/Down: select platform/game
- Return: open platform / download game
- Esc: games -> platforms
- Q: quit

The frontend accepts both the Amiga CSI byte (0x9b) and ANSI ESC-[ arrow sequences.
Game descriptions come from the ROMM `summary` field already parsed by libromm 0.5+.

### Important 0.7 limitation

The initial Amiga transport buffers an HTTP response before writing a download. This
keeps the first native transport small enough to validate networking and API behavior,
but is NOT the final low-memory A1200 download implementation. The next transport
revision should stream the response body directly to disk and handle HTTP/1.1 chunked
encoding. The host libcurl backend already streams downloads.

### 0.7
Diagnostic Amiga build for the observed 68020 Address Error. It adds trace markers,
uses the correct already-prefixed Authorization value, checks recv() failures and
request truncation, bounds response growth, and uses debug symbols with -O0.


## 0.7 Amiga transport changes

- Hardened receive-buffer arithmetic against `size_t` overflow.
- Checks negative `recv()` results instead of accepting partial responses.
- Validates request truncation and DNS address length.
- Metadata responses use one allocation: HTTP headers are removed in-place with `memmove()`.
- ROM downloads stream directly to disk instead of buffering the complete ROM in RAM.
- 64 KiB receive progress messages make slow classic-Amiga transfers observable.
- Keeps the Roadshow/SANA-II compatible `bsdsocket.library` transport; HTTPS remains intentionally unsupported on Amiga.

## CI artifacts (0.7 corrected)

The Jenkins pipeline builds and archives both Linux frontends (`romm-cli`, `romm-tui`) and the AmigaOS 68k frontend (`romm-amiga`). The Amiga build is always invoked explicitly with `make -f Makefile.amiga all` and uses `/opt/amiga/bin` in `PATH`.
