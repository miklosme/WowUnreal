# AzerothCore server

A server is required for authentication and gameplay testing, but not for offline maps, model viewers, or parser-only tests. WowUnreal targets the [AzerothCore 3.3.5a repository](https://github.com/azerothcore/azerothcore-wotlk).

Choose one route:

## Existing remote server

You need only the auth address, a valid account, and network access. Neither Docker nor a local AzerothCore checkout is required to run the client.

## Local containerized server

Install Git, Docker Engine or Docker Desktop, and the Docker Compose plugin, then follow the [official AzerothCore Docker guide](https://www.azerothcore.org/wiki/install-with-docker). Pin the tested AzerothCore commit rather than tracking a moving branch.

AzerothCore currently describes source installation as the supported route and Docker as experimental or limited-support. The container route is nevertheless useful for an isolated development server when that tradeoff is acceptable.

## Local native server

Follow AzerothCore's platform installation guide and install its compiler, database, and extraction dependencies. Current upstream guidance requires MySQL 8 or newer and does not support MariaDB.

Server-side extracted data must be produced by the same pinned AzerothCore version. It does not replace the original client MPQs required by WowUnreal.

## Network defaults

The 3.3.5a authentication service conventionally listens on port 3724, and the world service commonly uses port 8085. Treat both as configurable server defaults. Store account credentials locally under `Saved/`; never add them to Git or documentation.
