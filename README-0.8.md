# libromm 0.8.3

Amiga console client improvements:

- interactive scrolling platform/game browser from 0.8.2
- game detail screen is redrawn cleanly
- descriptions are word-wrapped to 72 columns
- transport debug output is hidden by default
- add `--debug` as optional fourth argument to show `[NET]` diagnostics
- Amiga network buffers remain heap allocated to keep classic AmigaDOS stack usage low

Normal:
`romm-amiga http://PROXY:PORT TOKEN`

Debug:
`romm-amiga http://PROXY:PORT TOKEN --debug`
