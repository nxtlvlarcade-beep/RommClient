# ROMM Client v1.0 candidate

Implemented in source: platform search, A-Z browsing, RAW `D` downloads,
`Enter` to request verified WHDLoad archives through gateway, ZIP preparation,
Amiga UnZip extraction and ROMM_LAUNCHER-based WHDLoad invocation.

Limitations: no automatic ADF/IPF-to-WHDLoad conversion, archives must contain
exactly one `.slave`, UnZip/WHDLoad are external Amiga dependencies. Search
A-Z currently uses substring matching, not a strict first-letter index.

Not a production release: Amiga cross-compilation and real-hardware tests pending.
