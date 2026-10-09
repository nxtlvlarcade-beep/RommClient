RetroWeb 1.1 REST replacement
============================
Apply the three files at repository root. This replaces the Telnet-screen HTML wrapper.
The Telnet server on port 2323 remains untouched.
Caddy :80 -> 127.0.0.1:8092; Caddy :8080 and index service :8091 remain intact.
The service uses ROMM_URL and ROMM_TOKEN from the existing gateway .env.
No JavaScript, cookies, ZMODEM or browser-side credentials.

Note: /cover forwards RomM's original image format (JPEG/GIF/PNG/WebP),
not converted to GIF. Old browsers lacking support may not display some covers.
The upstream cover path is taken from path_cover_small or path_cover_large.

No live RomM server or Unraid container was available for integration testing.
