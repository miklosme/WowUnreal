# WowUnreal — Project Overview

## Goal

A playable World of Warcraft 3.3.5a (Wrath of the Lich King) client targeting Unreal Engine 5.8.1, reading original MPQ data files, and connecting to AzerothCore servers.

## Design Priorities (in order)

1. **Visual Quality** — UE5 rendering (Lumen, Nanite where applicable, modern materials)
2. **High Performance** — 60+ FPS on mid-range hardware, efficient streaming
3. **Full Gameplay** — Complete client: combat, quests, dungeons, raids, PvP, addons
4. **Addon Compatibility** — Lua 5.1 + FrameXML for existing WoW 3.3.5 addons

## Architecture

```
Source/
├── WowData        — Binary format and MPQ parsing
├── WowAssets      — WoW-to-Unreal asset construction and caching
├── WowWorld       — Terrain, objects, sky, water, and audio
├── WowNetwork     — Authentication, world protocol, and entity state
├── WowUI          — Lua VM, FrameXML, widgets, and addons
├── WowClient      — Client state and credential handling
├── WowUnreal      — Game shell, controllers, and gameplay presentation
├── WowTests       — Unreal automation tests
└── ThirdParty     — StormLib, Lua 5.1.5, and pugixml
```

## Upstream status snapshot

The following labels were inherited with the specification. They have not been re-audited for this fork and are not the live backlog; confirm them in source, tests, and GitHub Issues.

### Reported implemented
- MPQ archive chain loading (StormLib, thread-safe, locale-aware)
- BLP→UTexture2D pipeline (DXT1/3/5 passthrough, paletted, mipmaps, cache)
- ADT terrain parsing + rendering (ProceduralMesh, 4-layer splatting, normals)
- WDT tile existence grid
- M2 static mesh parsing + doodad spawning
- WMO root+group parsing + rendering
- World streaming (camera-based tile load/unload)
- Auth networking (SRP6, ARC4-drop1024, realm list)
- World socket (encrypted packet framing)
- Generic DBC reader
- Lua 5.1.5 embedded (VM started, no API bindings yet)
- FrameXML parser skeleton
- TOC parser skeleton
- Screenshot manager (`UWowScreenshotManager`)

### Reported gaps
- Typed DBC wrappers (Map, AreaTable, Light, Spell, etc.)
- Terrain LOD / WDL distant terrain
- Water rendering (MH2O)
- Sky/atmosphere/day-night cycle
- M2 skeletal animation
- Character models + equipment
- ProceduralMesh → UStaticMesh migration
- Packet handlers (UPDATE_OBJECT, movement, combat, chat)
- Entity/GUID system
- Player movement + camera
- Lua API functions (~1200 needed)
- FrameXML widget system
- Addon loading
- Audio system
- Login/character select screens

## Proposed implementation tiers

This is retained prioritization guidance, not a committed schedule. Linked specifications exist in this directory; rows marked as planned areas have no retained standalone specification.

### Tier 1 — Foundation (Current → Playable Viewer)
| Spec | Description |
|------|-------------|
| [Static mesh](static-mesh.md) | ProceduralMesh → UStaticMesh/HISM migration |
| [Terrain LOD](terrain-lod.md) | Terrain LOD + WDL distant terrain |
| [Water](water.md) | Water/lava/slime rendering |
| [Sky and atmosphere](sky-atmosphere.md) | Sky, lighting, fog, day/night |
| [M2 animation](m2-animation.md) | M2 skeletal animation pipeline |
| [Character](character.md) | Character models + equipment |
| [DBC wrappers](dbc-wrappers.md) | Typed DBC table wrappers |

### Tier 2 — Playable Client
| Spec | Description |
|------|-------------|
| [Networking](networking.md) | Packet handlers + entity system |
| [Movement](movement.md) | Player movement + camera |
| [Lua API](lua-api.md) | Lua API bindings |
| [UI and FrameXML](ui-framexml.md) | FrameXML + widget system |
| Planned area | Chat system |
| Planned area | Combat display |

### Tier 3 — Feature Complete
| Spec | Description |
|------|-------------|
| Planned area | Bags, equipment, items |
| Planned area | Quest system |
| Planned area | Talent trees, spellbook |
| Planned area | Friends, guild, group |
| [Audio](audio.md) | Music, ambience, SFX |
| Planned area | World map, minimap |
| Planned area | Login, character select/create |

### Tier 4 — Polish
Loading screens, settings, WMO portal culling, ground clutter, weather, addon compat testing, performance pass, PvP, professions, dungeon finder, mail, auction house.

## Test Server

Use an AzerothCore 3.3.5a server selected through the [server setup](../setup/server.md). Localhost and ports 3724/8085 are conventional development defaults; accounts are local secrets and are not specified here.

## Verification Pattern

Every spec implementation must:

1. Build the relevant target using the current [development workflow](../setup/development.md).
2. Run the smallest map or automation test that exercises the behavior.
3. Capture an Unreal viewport screenshot for visual changes.
4. Check the project-local `Saved/Logs/` output for fatal errors.
