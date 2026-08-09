# AGENTS.md

WowUnreal is a work-in-progress reimplementation of the World of Warcraft 3.3.5a client. Read [docs/index.md](docs/index.md) before changing setup, architecture, protocol, or documentation behavior.

## Work routing

- Setup, build, game-data, or server work: read [docs/setup/README.md](docs/setup/README.md).
- Client behavior or subsystem requirements: read [docs/specs/README.md](docs/specs/README.md) and the relevant specification.
- External dependency or WoW-tool decisions: read [docs/research/external-dependencies-and-tools.md](docs/research/external-dependencies-and-tools.md).
- Historical status claims: confirm current behavior in source and GitHub Issues.

## Project workflow

Issues and live specifications are tracked in GitHub Issues with `gh`. See [docs/agents/issue-tracker.md](docs/agents/issue-tracker.md).

The repository uses the five canonical triage labels defined in [docs/agents/triage-labels.md](docs/agents/triage-labels.md).

Domain documentation uses the single-context layout described in [docs/agents/domain.md](docs/agents/domain.md).

## Data boundary

Blizzard assets and private credentials stay outside Git. Tests and launch commands receive the WoW data directory and server configuration from local runtime configuration.
