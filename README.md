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
