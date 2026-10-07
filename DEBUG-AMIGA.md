# libromm 0.7 Amiga debug build

This build adds D01-D11 trace points around OpenLibrary/gethostbyname/socket/htons/memcpy/connect.

Build:

    make -f Makefile.amiga clean
    make -f Makefile.amiga all

Artifacts:

- romm-amiga (debug symbols, -O0, frame pointers)
- romm-amiga.map (link map)

Test first with hostname:

    romm-amiga http://amigaproxy.nastc02.home x

Then with IP:

    romm-amiga http://192.168.0.7 x

Record the last `[NET Dxx]` line before a Guru.

If an exact program counter/address is available:

    m68k-amigaos-addr2line -f -C -e romm-amiga 0xADDRESS

Use the exact same romm-amiga binary that crashed.
