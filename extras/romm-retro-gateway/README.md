# libromm 1.0 — Retro Gateway

The retro gateway provides network-facing services for the terminal client and download workflows.

## Version 1.0 highlights

- Telnet-based terminal UI for RomM browsing.
- Visible interactive search prompt in the terminal UI.
- Game metadata and platform navigation.
- RAW and WHDLoad ZIP download workflows.
- Persistent download storage using the `/downloads` directory.

## Configuration

Create a `.env` file using `.env.example` in this directory as a reference. Configure the RomM connection and any service-specific settings described by that template. **Never commit `.env` or API tokens.**

## Build

Build from the **repository root** so Docker can access `examples/` and `extras/`:

    docker build --no-cache -f extras/romm-retro-gateway/Dockerfile -t romm-retro-gateway:1.0 .

## Example Unraid deployment

The example below assumes that the repository is checked out to `/mnt/user/nas-app/docker/appdata/RommClient-v1`, that the custom Docker network `br0` exists, and that `192.168.0.22` is reserved for this container.

    PROJECT=/mnt/user/nas-app/docker/appdata/RommClient-v1
    mkdir -p "$PROJECT/downloads"
    docker run -d \
      --name romm-retro-gateway \
      --restart unless-stopped \
      --network br0 \
      --ip 192.168.0.22 \
      --env-file "$PROJECT/extras/romm-retro-gateway/.env" \
      -v "$PROJECT/downloads:/downloads" \
      romm-retro-gateway:1.0

Do not use the same IP address for the host and container.

## Validation

    docker ps --filter name=romm-retro-gateway
    docker logs --tail 50 romm-retro-gateway
    docker inspect romm-retro-gateway --format '{{range .Mounts}}{{.Source}} -> {{.Destination}}{{println}}{{end}}'

Check that the host downloads directory is mounted at `/downloads` and verify browsing, search, and download operations from a client.

## Security

Run the Telnet gateway only on trusted networks. Telnet traffic is unencrypted. Protect the `.env` file and the RomM API token.
