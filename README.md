# libromm-simple 0.1

A deliberately small C prototype for talking to a RomM server from old and
modern systems.

## Goals

- Core code is plain C99.
- UI-independent.
- Network transport is injected through function pointers.
- Downloads stream directly to disk.
- A libcurl transport is included for easy testing.
- AmigaOS/MorphOS/Wii/etc. can later provide their own transport without
  rewriting the RomM-facing code.

## Important API note

RomM is actively developed and API/authentication details can vary by release.
This prototype therefore does **not** hard-code one claimed "universal" ROM
listing or download endpoint. The CLI accepts API paths explicitly.

Current RomM releases have dedicated third-party/client authentication
mechanisms and scoped client tokens. For a real client, prefer a RomM client
API token or the device authorization flow rather than storing a user's
password.

The tiny game-list JSON parser is intentionally a demo. It expects objects
with `id`, `name`, and `platform`. Real RomM responses may nest or name fields
differently; replace this parser with a small proper JSON parser once the
target RomM version is known.

## Build (Linux/macOS or another system with libcurl)

    make

## Test raw API access

    ./romm-cli http://romm-host:8080 YOUR_TOKEN raw /api/platforms

The exact endpoint depends on your RomM version. `raw` is useful for inspecting
the response before adapting the typed parser.

## Download

    ./romm-cli http://romm-host:8080 YOUR_TOKEN get /YOUR/DOWNLOAD/PATH game.zip

The file is streamed to disk rather than buffered in RAM, which is important
for Amiga-class targets.

## Porting to AmigaOS / MorphOS

Only `src/transport_curl.c` is tied to libcurl.

A native port can implement:

    romm_http_get_fn
    romm_http_download_fn
    romm_http_free_fn

and return those callbacks in a `romm_transport_t`.

The rest of `src/libromm.c` is ordinary C and deliberately avoids threads,
C++ STL, POSIX-only filesystem calls, and GUI dependencies.

For an AmigaOS 3.x build, the likely next step is a Roadshow/AmiTCP-compatible
HTTP transport (or an available curl build), plus AmiSSL if HTTPS is required.
For MorphOS, a libcurl-based transport is a natural first target.

## Suggested next milestones

1. Confirm the RomM server version.
2. Pair/create a scoped client token.
3. Capture real `/platforms` and ROM-list JSON responses.
4. Replace the demo parser with jsmn or another tiny parser.
5. Add pagination/search.
6. Add resumable downloads and progress callbacks.
7. Add launcher profiles independently of libromm.
8. Build a minimal AmigaOS 3.x GUI and a richer MorphOS MUI frontend.

## License

This prototype is provided under the MIT license; see LICENSE.
