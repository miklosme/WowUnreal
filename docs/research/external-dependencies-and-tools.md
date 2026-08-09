# External dependencies and WoW development tools

Research date: 2026-08-08; setup decision updated 2026-08-09

This note answers one narrow setup question: what must exist outside this repository to build and run WowUnreal, and which WoW-community projects are merely useful references or validation tools? It is based on the current checkout plus primary upstream documentation.

## Short answer

The minimum client-side stack is:

1. Unreal Engine **5.8.1**, the fork's selected development baseline.
2. The platform compiler/toolchain bundled with or supported by UE 5.8.1.
3. CMake plus a C/C++ build tool to produce the two static archives that this repository expects but does not commit: `libstorm.a` and `liblua.a`.
4. A user-supplied, complete **enUS World of Warcraft 3.3.5a build 12340 `Data/` tree**.

That is sufficient for offline maps, model viewers, and most data-driven tests. Full login and gameplay additionally need a reachable AzerothCore server and an account. A container runtime is **not** a WowUnreal dependency; Docker plus its Compose plugin are required only if the selected way to host AzerothCore locally is its container workflow.

## Mandatory for the WowUnreal client

| Dependency | Why it is required now | Installation boundary |
|---|---|---|
| Unreal Engine 5.8.1 | The project association and target defaults identify UE 5.8; the installed 5.8.1 patch release is the tested baseline. The Linux Development editor target builds successfully with it. | Use Epic's precompiled Linux installed build or build the 5.8.1 engine source. See [Unreal Engine setup](../setup/unreal-engine.md). |
| UE-supported compiler and SDK | This is a native C++ Unreal project. Epic's 5.8 requirements name Ubuntu 22.04/Rocky Linux 8 and clang 20.1.8 as the supported development baseline ([Epic Linux requirements](https://dev.epicgames.com/documentation/unreal-engine/linux-development-requirements-for-unreal-engine?lang=en-US)). | The precompiled UE 5.8.1 Linux archive bundles Epic's v26 clang 20.1.8 toolchain and .NET SDK. An IDE is optional. |
| CMake and a native build tool | [`WowData.Build.cs`](../../Source/WowData/WowData.Build.cs) expects `Source/ThirdParty/StormLib/build/libstorm.a`. StormLib source is vendored, but the archive is absent. The vendored upstream build is CMake-based; upstream also documents CMake builds ([StormLib repository](https://github.com/ladislav-zezula/StormLib)). | Host installation, used once initially and whenever the vendored dependency or toolchain changes. No second StormLib clone is needed. |
| C compiler, `make`, `ar`, and `ranlib` | [`WowUI.Build.cs`](../../Source/WowUI/WowUI.Build.cs) expects `Source/ThirdParty/lua/liblua.a`. Lua 5.1.5 source and its Makefile are vendored, but the archive is absent. Building only the `a` target avoids the readline/ncurses dependencies of the standalone Lua executable. | Host toolchain. No separate Lua repository is needed. |
| WoW 3.3.5a build-12340 game data | [`MpqManager.cpp`](../../Source/WowData/Private/Mpq/MpqManager.cpp) reads original MPQ archives at runtime. Its archive list currently spells out `enUS/...` names, so other locales are not setup-ready without code changes. Blizzard assets must remain user-supplied; AzerothCore's server-data container is not a replacement for this MPQ tree. | Outside Git and outside container images. Define its absolute path globally as `WOW_DATA`, then pass the shell-expanded value with `-wowdata="${WOW_DATA}"` until configuration is cleaned up. |

The repository already vendors StormLib 9.31 source, Lua 5.1.5 source, and pugixml 1.15 source. Pugixml is compiled into WowUI by [`PugiXmlWrapper.cpp`](../../Source/WowUI/Private/PugiXmlWrapper.cpp). [`WowNetwork.Build.cs`](../../Source/WowNetwork/WowNetwork.Build.cs) requests Unreal's own OpenSSL third-party module. These are **not** additional repositories to install.

The UE 5.8.1 precompiled Linux archive does not contain the `SetupToolchain.sh` named by Epic's generic Linux quickstart. Its compiler is already present; running the installed build's `Build.sh -help` is the relevant toolchain smoke check.

### Current platform caveat

The checked-in third-party build rules name Unix `.a` archives on every platform. The README's Windows build command is therefore not a verified Windows setup: Windows support needs explicit `.lib` production/selection (and likely per-platform dependency rules) before it can be promised. Pick one initial host platform and make that path reproducible before generalizing the setup guide.

## What is required to play against a server

WowUnreal needs an AzerothCore-compatible 3.3.5a endpoint, not necessarily a server installed on the same computer. The implementation and local specs target [AzerothCore's 3.3.5a server repository](https://github.com/azerothcore/azerothcore-wotlk).

Choose exactly one server route:

### Route A: an existing remote server

Required locally: only the server address, a valid account, and network access. Neither Docker nor an AzerothCore clone is required to run the client. A local AzerothCore clone remains useful as protocol reference source.

### Route B: a local containerized AzerothCore

Required: Git, Docker Engine or Docker Desktop, and the Docker Compose plugin, plus an `azerothcore-wotlk` clone. The official Docker guide says its software requirements are Git, Docker, and Compose, and that its client-data container populates the **server-side** data volume ([AzerothCore Docker guide](https://www.azerothcore.org/wiki/install-with-docker)).

This is the most isolated setup for WowUnreal client work, but document its support status honestly: AzerothCore's current installation chooser calls source installation the supported/recommended route and categorizes Docker as experimental/limited support ([AzerothCore installation guide](https://www.azerothcore.org/wiki/installation)). Pin the tested AzerothCore commit in WowUnreal documentation so a moving `master` does not silently change the test environment.

### Route C: a local native AzerothCore build

Required: the complete AzerothCore native compiler/dependency stack and database server in its platform guide, followed by matching server-data extraction. This avoids Docker but is a substantially larger host setup. Current upstream diagnostics require MySQL 8 or newer and say MariaDB is unsupported ([AzerothCore common errors](https://www.azerothcore.org/wiki/common-errors)). Do not combine old extractor binaries or data from another emulator with a current AzerothCore checkout; upstream explicitly marks old extractors deprecated and other cores' generated map data incompatible ([AzerothCore FAQ](https://www.azerothcore.org/wiki/faq)).

For this fork's first reproducible setup, Route B is the pragmatic default if limited upstream support is acceptable; Route A is the smallest setup when a stable test server already exists.

## Optional tools worth keeping in the ecosystem

These projects should not be installed as WowUnreal build dependencies. Use them as separate sibling checkouts or standalone applications, with a recorded purpose.

| Priority | Tool | Best use here | Status and caution |
|---|---|---|---|
| 1 | [wow.export](https://github.com/Kruithne/wow.export) | Browse legacy MPQs, preview M2/WMO models, inspect maps, and export reference renders/geometry for visual comparison. Its current README explicitly lists legacy MPQ browsing, M2/WMO preview, and an overhead map viewer. | Recommended standalone validation tool. Exported/transcoded assets are not canonical parser truth and must not be committed as Blizzard assets. This is likely the most useful “community-built map viewer” for day-to-day comparison. |
| 2 | [Noggit3](https://github.com/wowdev/noggit3) | Inspect 3.3.5a terrain, ADT/WMO/M2 placement behavior, lighting, and map-editing edge cases. Upstream describes it as the premier map editor specifically for 3.3.5a. | Recommended source reference and optional editor. Not required merely to view WowUnreal's world. |
| 3 | [WMVx](https://github.com/Frostshake/WMVx) | Cross-check WotLK M2 animation, character equipment, attachment points, geosets, and model rendering. Upstream explicitly lists WotLK 3.3.5 support. | Recommended viewer/reference, but its README records known WotLK texture-animation and transparency issues, so do not treat screenshots as unquestionable ground truth. |
| 4 | [pywowlib](https://github.com/wowdev/pywowlib) | A readable independent implementation for ADT/WMO/M2 and related format experiments; useful for small inspection/conversion scripts. | Reference-only. Verify inferred layouts against files, WowDev documentation, and another implementation before copying them into C++. |
| 5 | [WowDev Wiki](https://wowdev.wiki/Main_Page) and the [wowdev organization](https://github.com/wowdev) | File-format terminology, structure documentation, listfiles, and related libraries. | Essential research bookmarks, not installation prerequisites. Prefer format pages and source definitions over unsourced forum summaries. |
| 6 | [AzerothCore source](https://github.com/azerothcore/azerothcore-wotlk) | Canonical target-server behavior for SRP/authentication, opcodes, packet layouts, DBC structures, and gameplay expectations. | Mandatory clone only for local source/container hosting; otherwise a highly recommended pinned sibling checkout. Do not copy server-authoritative behavior into the client without checking the protocol boundary. |
| 7 | [TrinityCore's 3.3.5 branch](https://github.com/TrinityCore/TrinityCore/tree/3.3.5) | A second implementation when AzerothCore behavior is ambiguous, especially for protocol archaeology. | Optional second opinion, not this project's target backend. Differences must be resolved in favor of observed AzerothCore compatibility. |

Blender is useful only when investigating exported geometry/materials, most conveniently through wow.export's Blender add-on; it is not necessary to build or run WowUnreal. A SQL client such as DBeaver is similarly optional for inspecting a local AzerothCore database.

## Historical, obsolete, or deliberately excluded

- The original [WoW Model Viewer repository](https://github.com/wowmodelviewer/wowmodelviewer) remains useful for historical M2 rendering code, but its current project page does not claim a maintained 3.3.5 workflow. Prefer WMVx for a live WotLK-aware viewer and keep the old project reference-only.
- The local spec lists WowGodot as “visual validation only (last resort),” while the old agent notes warn that it has known issues. Do not include it in the setup baseline.
- Do not download random prebuilt AzerothCore map/vmap/mmap/DBC bundles or old extractor packages. Use the data provisioned by the pinned container workflow or extract with tools built from the same pinned server checkout.
- TrinityCore's current [WowPacketParser](https://github.com/TrinityCore/WowPacketParser) main tree does not expose a 3.3.5/build-12340 module in its listed modules. Do not make it a recommended dependency until an explicitly maintained 3.3.5-compatible revision and capture workflow are identified.
- WoWDBDefs is valuable for modern client databases, but its [upstream scope currently says definitions start at 7.3.5 while older data is being backfilled](https://github.com/wowdev/WoWDBDefs). It should not replace the existing 3.3.5 DBC layouts as a setup dependency.

## Reproducibility gaps to close in the setup plan

1. Complete runtime editor/map validation under UE 5.8.1. Generate per-platform third-party library paths before advertising Windows support.
2. Replace all creator-specific defaults with command-line/config/environment discovery. In particular, [`WowWorldManager.cpp`](../../Source/WowWorld/Private/WowWorldManager.cpp) still falls back to the original author's macOS data path.
3. Add a non-secret sample connection configuration and keep real credentials under `Saved/`/Git ignore.
4. Record a tested AzerothCore commit and the exact server route. Document account creation, realm address, ports, health checks, and teardown without baking test credentials into source.
5. Add a setup doctor that reports UE, compiler, CMake, static archives, MPQ archive completeness/locale, and server reachability before attempting to launch.

## Verified local engine state

The precompiled UE 5.8.1 Linux installed build was verified at setup time: UnrealBuildTool starts, the archive reports changelist 56057345, and the bundled compiler reports clang 20.1.8. NVIDIA driver 595.84 successfully detects the development machine's RTX 4070 Ti. The checked-in bootstrap successfully produced and validated both project-specific static archives with that toolchain.
