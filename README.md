# libromm 1.0

libromm is a C client project for browsing a RomM library from retro and terminal environments.

## Release 1.0 highlights

- Native AmigaOS 3.x client for platform and game browsing.
- Terminal-based TUI for browsing platforms, games, and metadata.
- Interactive game search with visible text input.
- RAW downloads and gateway-assisted WHDLoad ZIP downloads.
- Docker retro gateway with a persistent `/downloads` mount.
- Unified application title: `libromm-1.0`.

## Repository components

- `examples/romm_amiga.c` — native Amiga client.
- `examples/romm_tui.c` — terminal user interface.
- `extras/romm-retro-gateway/` — Docker-based gateway and supporting services.

## Building the terminal client

From the repository root:

    make clean
    make -j2

## Building the Amiga client

Use the Amiga toolchain and build environment configured for this project:

    make -f Makefile.amiga clean
    make -f Makefile.amiga -j2

## Building the Docker gateway

Run this command from the **repository root** (not from the gateway subdirectory):

    docker build --no-cache -f extras/romm-retro-gateway/Dockerfile -t romm-retro-gateway:1.0 .

See [Gateway README](extras/romm-retro-gateway/README.md) for an example deployment.

## Testing

Retain and run the project's existing tests and build checks. Before publishing a release, verify the Linux/TUI build, the Amiga build, gateway startup, platform and game browsing, interactive search, and downloads.

## Security

Keep RomM API tokens in an untracked `.env` file. Do not expose the plaintext Telnet service directly to the public internet. Prefer a trusted local network.

## Release

Version: **1.0**  
Git tag: **`v1.0`**
