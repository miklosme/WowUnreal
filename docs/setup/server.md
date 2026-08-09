# AzerothCore server

A server is required for authentication and gameplay testing, but not for offline maps, model viewers, or parser-only tests. WowUnreal targets the [AzerothCore 3.3.5a repository](https://github.com/azerothcore/azerothcore-wotlk).

Choose one route:

## Existing remote server

You need only the auth address, a valid account, and network access. Neither Docker nor a local AzerothCore checkout is required to run the client.

## Local containerized server

Install Git, Docker Engine or Docker Desktop, and the Docker Compose plugin. The tested local server is pinned to AzerothCore commit `ff7fb4cc8bbb6dbcc0a50d6cbdb720a7b0b24fdd` (2026-08-09). Clone it beside WowUnreal and detach the checkout at that commit:

```bash
git clone https://github.com/azerothcore/azerothcore-wotlk.git ../azerothcore-wotlk
git -C ../azerothcore-wotlk switch --detach ff7fb4cc8bbb6dbcc0a50d6cbdb720a7b0b24fdd
```

Build and start the stack from the AzerothCore checkout:

```bash
cd ../azerothcore-wotlk
docker compose up -d --build
docker compose ps -a
```

The first run compiles AzerothCore, initializes MySQL, imports the three databases, and populates the server-data volume. `ac-database`, `ac-authserver`, and `ac-worldserver` should remain running. `ac-db-import` and `ac-client-data-init` are one-shot containers and should report `Exited (0)` after successful initialization.

Create a non-GM local account from the worldserver console, substituting local credentials:

```bash
docker attach ac-worldserver
account create YOUR_ACCOUNT YOUR_PASSWORD
```

Detach without stopping the server by pressing `Ctrl-p`, then `Ctrl-q`. Put the credentials in the ignored `Saved/WowCredentials.json` file described in [runtime configuration](configuration.md); do not commit or document the password.

The default realm is named `AzerothCore` and advertises `127.0.0.1:8085`. Verify the local listeners with:

```bash
nc -zv 127.0.0.1 3724
nc -zv 127.0.0.1 8085
```

Stop and restart the persistent services without deleting their database or server-data volumes:

```bash
docker compose stop
docker compose start
```

`docker compose down` removes the containers and network but preserves the named volumes. Adding `--volumes` deletes the local databases and downloaded server data and requires a full first-run initialization next time.

AzerothCore currently describes source installation as the supported route and Docker as experimental or limited-support. The container route is nevertheless useful for an isolated development server when that tradeoff is acceptable.

## Local native server

Follow AzerothCore's platform installation guide and install its compiler, database, and extraction dependencies. Current upstream guidance requires MySQL 8 or newer and does not support MariaDB.

Server-side extracted data must be produced by the same pinned AzerothCore version. It does not replace the original client MPQs required by WowUnreal.

## Network defaults

The 3.3.5a authentication service conventionally listens on port 3724, and the world service commonly uses port 8085. Treat both as configurable server defaults. Store account credentials locally under `Saved/`; never add them to Git or documentation.
