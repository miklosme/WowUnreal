# WowUnreal — Project Overview

## Goal

A fully playable World of Warcraft 3.3.5a (Wrath of the Lich King) client built on Unreal Engine 5.7 that reads original MPQ data files and connects to AzerothCore servers.

## Design Priorities (in order)

1. **Visual Quality** — UE5 rendering (Lumen, Nanite where applicable, modern materials)
2. **High Performance** — 60+ FPS on mid-range hardware, efficient streaming
3. **Full Gameplay** — Complete client: combat, quests, dungeons, raids, PvP, addons
4. **Addon Compatibility** — Lua 5.1 + FrameXML for existing WoW 3.3.5 addons

## Architecture

```
WowUnreal/
├── WowData        — Binary format parsers (ADT, WDT, WDL, M2, WMO, BLP, DBC, MPQ)
├── WowAssets      — Asset conversion (BLP→Texture, M2→Mesh, cache management)
├── WowWorld       — World streaming, terrain rendering, object management
├── WowRenderer    — Materials, shaders, lighting, sky, water, particles
├── WowCharacter   — Player/NPC models, equipment, animations, mounts
├── WowGameplay    — Combat, spells, auras, inventory, quests, talents
├── WowNetwork     — Auth/world sockets, packet handlers, state sync
├── WowUI          — Lua VM, FrameXML, widget system, addon loader
├── WowAudio       — Music, ambience, sound effects
├── WowClient      — Login flow, character select, settings, screenshots
└── ThirdParty     — StormLib, Lua 5.1.5
```

## Current Status

### Done
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

### Not Started
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

## Implementation Tiers

### Tier 1 — Foundation (Current → Playable Viewer)
| Spec | Description |
|------|-------------|
| `static-mesh.md` | ProceduralMesh → UStaticMesh/HISM migration |
| `terrain-lod.md` | Terrain LOD + WDL distant terrain |
| `water.md` | Water/lava/slime rendering |
| `sky-atmosphere.md` | Sky, lighting, fog, day/night |
| `m2-animation.md` | M2 skeletal animation pipeline |
| `character.md` | Character models + equipment |
| `dbc-wrappers.md` | Typed DBC table wrappers |

### Tier 2 — Playable Client
| Spec | Description |
|------|-------------|
| `networking.md` | Packet handlers + entity system |
| `movement.md` | Player movement + camera |
| `lua-api.md` | Lua API bindings |
| `ui-framexml.md` | FrameXML + widget system |
| `chat.md` | Chat system |
| `combat.md` | Combat display |

### Tier 3 — Feature Complete
| Spec | Description |
|------|-------------|
| `inventory.md` | Bags, equipment, items |
| `quests.md` | Quest system |
| `talents.md` | Talent trees, spellbook |
| `social.md` | Friends, guild, group |
| `audio.md` | Music, ambience, SFX |
| `maps.md` | World map, minimap |
| `login.md` | Login, character select/create |

### Tier 4 — Polish
Loading screens, settings, WMO portal culling, ground clutter, weather, addon compat testing, performance pass, PvP, professions, dungeon finder, mail, auction house.

## Test Server

- Host: `127.0.0.1`
- Auth port: 3724, World port: 8085
- Test account: `WowTestUser` / `WowTestPass`
- Server: AzerothCore 3.3.5a

## Verification Pattern

Every spec implementation must:
1. **Build** — `./run_test.sh build` succeeds
2. **Run** — `./run_test.sh` launches without crashes
3. **Screenshot** — Visual verification via `UWowScreenshotManager::TakeScreenshot()`
4. **Log check** — No fatal errors in `~/Library/Logs/WowUnreal/WowUnreal.log`
