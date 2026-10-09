# libromm 1.1 — Retro Gateway

The Docker gateway provides a retro-friendly RomM web frontend, Telnet TUI, and download services. **No `.env` editing is needed for a new installation.**

## Quick installation (Docker / Unraid)

Build from the **repository root** (the Docker build context must be `.`):

```sh
docker build -f extras/romm-retro-gateway/Dockerfile -t romm-retro-gateway:1.1-rest .
```

Choose a host directory for persistent downloads and settings. This example uses Unraid with an existing `br0` network and a **reserved, unused** container IP `192.168.0.22`:

```sh
APP=/mnt/user/nas-app/docker/appdata/RommClient-v1
mkdir -p "$APP/downloads"
docker run -d \
  --name romm-retro-gateway \
  --restart unless-stopped \
  --network br0 --ip 192.168.0.22 \
  -v "$APP/downloads:/downloads" \
  romm-retro-gateway:1.1-rest
```

Do not assign the host's own IP to the container. For other Docker networks, adapt the networking and publish the necessary ports explicitly. A dedicated `br0` IP does not require `-p` port mappings.

## First run: configure in the browser

1. Start the container as shown above. No `--env-file` is required.
2. Read the generated **setup password** from the container console/log:

   ```sh
   docker logs romm-retro-gateway 2>&1 | grep 'RetroWeb configuration password'
   ```

3. Open **`http://192.168.0.22/config`** (replace the IP with your gateway address).
4. Log in using **username `admin`** and the password from the log.
5. Enter the **RomM URL** (e.g. `https://romm.example.local`) and **RomM API token**. Optionally set the **Telnet port** (default `2323`) and enable/disable **ZMODEM**.
6. Select **Save and test**. The gateway verifies the RomM connection, stores the settings, and reloads the services automatically (allow a few seconds).
7. Open **`http://192.168.0.22/`** for RetroWeb. Connect via Telnet to `192.168.0.22:2323` or your configured port. Gateway API/download service uses port `8080`.

Settings and the generated setup password persist in `/downloads/.retro-config/` (inside the mounted host directory). The configuration file is `romm.env`. Keep this directory private and **do not commit it to Git**. The token is not displayed back in the configuration form; leave the token field blank to retain the saved token.

**Changing the Telnet port:** save the new port at `/config`, wait for the services to restart, and reconnect to the new port. If you use port publishing rather than a dedicated container IP, update Docker's published port mapping as well.

**Existing installations:** keep the same `/downloads` mount when replacing the container. The web configuration stored there takes precedence over startup environment values. Existing `.env` deployments may continue using `--env-file`, but the web setup is the recommended first-run workflow.

## Verify / troubleshoot

```sh
docker ps --filter name=romm-retro-gateway
docker logs --tail 100 romm-retro-gateway
curl -i http://192.168.0.22/
curl -sS -D /tmp/cover.headers -o /tmp/cover.bin 'http://192.168.0.22/cover?id=ROM_ID'
cat /tmp/cover.headers
file /tmp/cover.bin
```

Replace `ROM_ID` with an actual ID from a RetroWeb game page. For RomM 5.3.x cover URLs, the gateway URL-encodes spaces in timestamp query parameters (`?ts=...`) before fetching images; otherwise Python can raise `InvalidURL` and Caddy returns 502.

## Security and compatibility

- `/config` is protected with HTTP Basic authentication, **not TLS**. Credentials can be intercepted on untrusted networks. Restrict to a trusted LAN or use HTTPS plus appropriate access controls.
- Telnet is unencrypted. Never expose it directly to the public internet.
- The configuration password is printed to container logs at startup; protect access to Docker logs and the mounted configuration directory.
- RetroWeb uses simple HTML without JavaScript, suitable for older browsers. Cover and screenshot images are served through the gateway.
