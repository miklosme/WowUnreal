# WowUnreal

WowUnreal is a work-in-progress reimplementation of the World of Warcraft: Wrath of the Lich King client in Unreal Engine. It reads user-supplied WoW 3.3.5a data at runtime and targets AzerothCore-compatible servers. The repository contains no Blizzard game assets.

## Current status

This fork is being made reproducible on Linux before feature development continues.

- The project metadata targets Unreal Engine 5.8, and the `WowUnrealEditor` Linux Development target builds successfully with the installed UE 5.8.1 baseline.
- The checked-in build and runtime launchers support Linux using `UE_ROOT`, `WOW_DATA`, and repository-relative paths.
- StormLib, Lua 5.1.5, and pugixml sources are vendored. Generate the ignored `libstorm.a` and `liblua.a` outputs with the checked-in Linux dependency bootstrap before the first project build.
- Windows is not currently a supported build target because the third-party build rules only select Unix static archives.

The Linux editor setup and AzerothCore-backed gameplay path have been validated,
but the client itself remains incomplete.

## Required inputs

- Unreal Engine 5.8.1 for the fork's target environment. See [Unreal Engine setup](docs/setup/unreal-engine.md).
- A supported C++ toolchain and the native build dependencies listed in [development setup](docs/setup/development.md).
- A legally obtained, unmodified WoW 3.3.5a build 12340 `Data/` directory containing the original MPQ archives. See [game data](docs/setup/game-data.md).
- A reachable AzerothCore 3.3.5a server and account for login/gameplay testing. Offline viewers and many parser tests do not require a server. See [server setup](docs/setup/server.md).

The current MPQ loader explicitly names the `enUS` locale archives. Other locales require a loader change before they can be considered supported.

## Repository layout

```text
WowUnreal/
├── Config/              Unreal project configuration
├── Content/             Project-owned Unreal maps and materials
├── Source/
│   ├── ThirdParty/      Vendored StormLib, Lua, and pugixml sources
│   ├── WowAssets/       WoW-to-Unreal asset construction
│   ├── WowClient/       Client state and credential handling
│   ├── WowData/         MPQ and WoW file-format parsing
│   ├── WowNetwork/      3.3.5a authentication and world protocol
│   ├── WowTests/        Unreal automation tests
│   ├── WowUI/           Lua, FrameXML, and UI widgets
│   ├── WowUnreal/       Game shell and gameplay controllers
│   └── WowWorld/        Terrain, world objects, sky, water, and audio
├── Scripts/             Linux build, launch, and dependency utilities
├── docs/                Setup, specifications, notes, and research
└── WowUnreal.uproject
```

## Documentation

Start with the [documentation index](docs/index.md). In particular:

- [Setup](docs/setup/README.md)
- [Technical specifications](docs/specs/README.md)
- [External dependencies and WoW development tools](docs/research/external-dependencies-and-tools.md)
- [Agent and issue-tracker conventions](AGENTS.md)

GitHub Issues are the live source of truth for planned work. Specifications describe intended behavior and should not be read as implementation status.

## Legal

World of Warcraft and related names and assets are trademarks or property of Blizzard Entertainment. This project is not affiliated with or endorsed by Blizzard Entertainment. Users must supply their own lawful game data, and Blizzard assets must not be committed to this repository.

This repository currently has no top-level software license. Add one before representing the project as licensed for redistribution or contribution.
