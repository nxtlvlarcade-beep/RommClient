# libromm 0.8.1 - Amiga interactive client

0.8.1 keeps the existing Amiga client functions and makes the already present
platform/game/detail/download flow usable as a persistent interactive client.

Amiga controls:
- Up/Down: select
- Return on platform: open games
- Return on game: download selected ROM
- Esc: back to platforms
- Q: quit

Important fix in 0.8.1:
The large receive/download buffers are allocated on the heap instead of the
small classic AmigaDOS process stack. A manual `Stack 65536` should therefore
no longer be required just for these buffers.
