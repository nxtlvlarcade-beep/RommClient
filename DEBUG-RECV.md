# Amiga recv debug build

This build narrows metadata receive calls to 512 bytes and prints trace points around `recv`, buffer growth, and copying.

Expected sequence:

- `[NET R01]` immediately before `recv()`
- `[NET R02]` immediately after `recv()`
- `[NET R03]` before capacity/overflow checks
- `[NET R04]` after buffer allocation/growth
- `[NET R05]` after copying received bytes

If the machine crashes after R01 but before R02, the failure is inside the socket `recv()` call. If it reaches R02, the next trace identifies whether allocation/copying is involved.
