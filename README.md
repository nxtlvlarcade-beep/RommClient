# libromm 0.3

Portable C99 RomM client prototype.

## New in 0.3

- `games PLATFORM_ID [LIMIT] [OFFSET]`
- `search PLATFORM_ID TEXT [LIMIT]`
- typed `romm_game_t` / `romm_game_list_t`
- paginated `/api/roms` support
- correct RomM platform filter: `platform_ids` (plural)
- client-side case-insensitive search, deliberately avoiding version-specific
  server search parameters
- still builds portable `libromm.a` separately from `libromm-curl.a`

## Build

    make

## Examples

    export ROM_TOKEN='...'
    ./romm-cli https://romm.example "$ROM_TOKEN" platforms
    ./romm-cli https://romm.example "$ROM_TOKEN" games 21
    ./romm-cli https://romm.example "$ROM_TOKEN" games 21 50 0
    ./romm-cli https://romm.example "$ROM_TOKEN" search 21 Turrican
    ./romm-cli https://romm.example "$ROM_TOKEN" search 21 "Monkey Island" 20

Platform 21 was the Amiga platform in the test server supplied during development.

## Architecture

`libromm.a` remains independent of libcurl. `libromm-curl.a` is only one
transport implementation. This is intentional for later AmigaOS 3.x,
MorphOS and other ports.

The ROM list parser accepts both a direct JSON array and paginated object
responses with an `items` (or fallback `roms`) array.

## Next

0.4 should add ROM detail, discover/verify the content-download endpoint from
the target server's OpenAPI spec, then implement download-by-ROM-ID with
progress callbacks.
