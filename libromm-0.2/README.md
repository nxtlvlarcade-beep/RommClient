# libromm 0.2

Portable C99 RomM client prototype.

This version uses the real `/api/platforms` response supplied during development
and exposes a typed platform list. It extracts platform id, slug, name,
display name, ROM count, category, generation, family, filesystem size and
status flags while safely skipping nested data such as firmware arrays.

Build:

    make

Test:

    export ROM_TOKEN='...'
    ./romm-cli https://romm.example "$ROM_TOKEN" platforms

Raw API inspection remains available:

    ./romm-cli https://romm.example "$ROM_TOKEN" raw /api/platforms

The portable core is `libromm.a`. curl is isolated in `libromm-curl.a`, so an
AmigaOS/MorphOS transport can replace it later.
