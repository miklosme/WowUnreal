# WowUnreal

A World of Warcraft 3.3.5a (Wrath of the Lich King) client built on Unreal Engine 5.7. Reads original WoW MPQ data files at runtime, connects to AzerothCore private servers, and provides full gameplay with addon support via Lua scripting. Ships with no Blizzard assets.

---

## Table of Contents

- [Overview](#overview)
- [Key Specifications](#key-specifications)
- [Architecture](#architecture)
- [Features](#features)
- [Prerequisites](#prerequisites)
- [Quick Start](#quick-start)
- [Project Structure](#project-structure)
- [Configuration](#configuration)
- [Development](#development)
- [Status](#status)
- [Contributing](#contributing)
- [Legal](#legal)

---

## Overview

WowUnreal is a faithful recreation of the World of Warcraft client for the 3.3.5a expansion (Wrath of the Lich King). It demonstrates modern game engine architecture (Unreal Engine 5.7) applied to reverse-engineered WoW protocols and asset formats. The project is entirely written in C++, uses no Blueprints for core logic, and maintains compatibility with AzerothCore 3.3.5a private servers.

The client reads unmodified WoW MPQ archives and converts them into Unreal Engine native formats at runtime. It implements the full WoW 3.3.5a network protocol (121+ opcodes), Lua 5.1.5 scripting engine with FrameXML UI system, and comprehensive gameplay mechanics including combat, questing, social systems, and addon support.

---

## Key Specifications

| Specification | Value |
|---|---|
| **Engine** | Unreal Engine 5.7 |
| **Language** | C++ (no Blueprints) |
| **Protocol Version** | WoW 3.3.5a (build 12340) |
| **Scripting** | Lua 5.1.5 (original WoW version) |
| **Server Backend** | AzerothCore 3.3.5a |
| **Primary Platform** | macOS |
| **Secondary Platform** | Windows (planned) |
| **Data Source** | Original WoW MPQ files (user-supplied) |

### Third-Party Libraries

- **StormLib** — MPQ archive reading
- **Lua 5.1.5** — Scripting engine
- **pugixml** — XML parsing (FrameXML)
- **OpenSSL** — Cryptography (SRP6, ARC4)

---

## Architecture

WowUnreal is organized into 8 modular systems:

### WowData
Binary format parsers for all WoW file types:
- ADT (terrain tiles with heightmaps, normal maps, blending layers)
- WDT (world definition and LOD reference)
- WDL (world low definition for distant terrain)
- M2 (skeletal character and doodad models with 150+ animation IDs)
- WMO (static world buildings with materials and doodad groups)
- BLP (DXT-compressed textures)
- DBC (game database files)
- MPQ (archive format via StormLib)

### WowAssets
Asset conversion pipeline that transforms WoW formats into Unreal Engine 5 native assets:
- BLP → UTexture2D (with DXT decompression)
- M2 → USkeletalMesh (with bone hierarchy and animation tracks)
- WMO → UStaticMesh (with collision and materials)
- BLP texture layers → UMaterial instances (4-layer splatting for terrain)
- DBC → data structures (game database)

### WowWorld
World streaming and rendering system:
- Terrain tile streaming with LOD system (full detail → LOD1 → WDL distant)
- 4-layer terrain splatting with blending
- WMO building placement and rendering
- M2 doodad instanced rendering with distance-based streaming
- Sky/atmosphere with zone-specific Light.dbc blending
- Day/night cycle with sun/moon rendering
- Water and ocean rendering with animation
- Fog system per zone
- Zone music (MP3) and ambient sound (WAV) streaming

### WowUI
Lua 5.1 virtual machine integrated with WoW UI framework:
- FrameXML parser (5200+ frames from original WoW)
- Addon loader with TOC parsing and dependency resolution
- Widget system (buttons, frames, textures, animations)
- Event system matching original WoW API
- 375+ Lua API functions for UI scripting
- WoW-themed login screens with expansion tabs
- Cursor manager with BLP texture cursors

### WowNetwork
Full network implementation for WoW 3.3.5a protocol:
- SRP6 authentication socket with challenge-response
- ARC4 encryption for world socket
- 121+ opcode handlers for game messages
- Entity system (character position, animation, targeting)
- Movement authority and validation
- Spell cast and combat result handling
- Chat routing (Say, Yell, Party, Guild, Whisper)
- Social system (friends, ignores, guild)
- Party and group management

### WowClient
Client-side systems for user experience:
- Login flow: login screen → realm select → character select/create → world entry
- Credential storage (JSON-based, local to user)
- Engine and graphics settings management
- Account and character persistence

### WowUnreal
Game shell and gameplay controller:
- Game mode and player character controller
- Player movement (WASD, jump, swimming)
- Third-person camera with orbit and zoom
- Targeting system (click-to-target, tab cycling)
- Auto-attack and spell casting mechanics
- Cast bar and GCD (Global Cooldown) management
- Floating damage text display
- Death screen and spirit release
- Loading screen display

### WowTests
Unit test suite for data parsers and asset conversion. Validates format correctness and rendering accuracy.

---

## Features

### Data Pipeline
- Automatic MPQ archive chain discovery and reading
- Streaming asset conversion (on-demand from disk)
- Texture streaming with mipmap generation
- Skeletal mesh streaming with bone LOD
- DBC database lazy loading

### World Rendering
- Terrain with 4-layer splatting and alpha blending
- LOD system: full detail terrain tiles → LOD1 → distant WDL
- WMO building rendering with per-material shaders
- M2 doodad instancing with view frustum culling
- Sky dome with time-of-day and zone blending
- Water surface with animated UV scrolling
- Fog and atmospheric effects per zone
- Sun, moon, and stars rendering

### Character System
- 10 playable races × 2 genders
- Composite texture generation from layer files
- Geoset visibility (e.g., hide armor under robes)
- Equipment rendering (head, shoulder, chest, back, hands, legs, feet, wrist)
- Hair, skin, and facial feature customization
- Full skeletal animation with 150+ animation IDs
- Animation state machine (idle, run, jump, cast, death, etc.)

### Gameplay Mechanics
- Player movement: WASD, sprint, jump, swimming, auto-run
- Combat system: auto-attack, melee and ranged damage
- Spell casting with spell learning progression
- Cast bar with animation syncing
- Global Cooldown (GCD) enforcement
- Talent tree UI and point allocation
- Threat/aggro system
- Death and spirit release mechanics
- Loot windows and item looting

### User Interface
- Dynamic UI from FrameXML (5200+ frames)
- Addon system with dependency resolution
- Action bar with hotkey support and cooldown display
- Spellbook with spell categories
- Character sheet (stats, resistances, talents)
- Inventory and bag system
- Equipment slot display
- Quest log and quest dialog
- Chat window with multiple channels
- Party/group UI
- Friends list and guild roster
- Minimap with POI markers
- Nameplates with health bars above characters
- Buff/debuff display
- Experience bar with level progression

### Network & Protocol
- Full SRP6 authentication protocol
- ARC4 encryption for all world packets
- 121+ opcode handlers for game messages
- Player entity synchronization (position, rotation, animation)
- Movement validation and speed checks
- Spell queuing and cast result handling
- Damage notifications and combat log
- Zone transitions and world changes
- Warden anti-cheat handler (graceful acceptance)
- Loading screen progression

### Audio & Localization
- Zone music playback (MP3 format)
- Ambient sound effects (WAV format)
- Character voice emotes
- Ability sound effects
- Music fade and transition
- Locale-aware text rendering

### Addon Support
- Full Lua 5.1.5 compatibility
- FrameXML parsing from original WoW
- Addon TOC file parsing
- Dependency resolution and load ordering
- Event binding and firing
- Secure command execution for macros
- Slash command routing

---

## Prerequisites

### System Requirements
- **macOS 11+** (primary platform) or **Windows 10/11** (secondary)
- **16GB RAM** (minimum for streaming)
- **SSD** recommended for asset streaming performance
- **GPU**: Metal (macOS) or DirectX 12 (Windows) capable card

### Software
- **Unreal Engine 5.7** (installed via Epic Games Launcher)
- **Xcode 13+** (macOS) or **Visual Studio 2022** (Windows)
- **Python 3.8+** (optional, for build scripts)

### Game Data
- **World of Warcraft 3.3.5a client data** (user must supply)
  - Required: `/Data` folder containing MPQ files
  - The game does NOT include any Blizzard assets
  - You must own a valid copy or have access to the original WoW installation
  - Default path: `~/Downloads/World of Warcraft 3.3.5a/Data`
  - Customizable via `-wowdata="<path>"` launch argument

### Server
- **AzerothCore 3.3.5a server** to connect to
- Server address configured in `Saved/WowCredentials.json`
- Account credentials stored locally (encrypted recommended)

---

## Quick Start

### 1. Clone and Verify

```bash
git clone https://github.com/yourusername/WowUnreal.git
cd WowUnreal
```

### 2. Point to WoW Data

Move or symlink your WoW 3.3.5a data folder:

```bash
# Option A: Copy/move your WoW Data folder
cp -r "/path/to/World of Warcraft 3.3.5a/Data" ~/Downloads/

# Option B: Create symlink
ln -s "/path/to/World of Warcraft 3.3.5a/Data" ~/Downloads/
```

Or at runtime, specify the path:

```bash
./run_game.sh -wowdata="/your/custom/path/Data"
```

### 3. Build

From the project root:

```bash
# macOS
./build.sh

# Or manually
UE="/Users/Shared/Epic Games/UE_5.7"
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" WowUnreal Mac Development \
  -project="$(pwd)/WowUnreal.uproject" -waitmutex
```

### 4. Configure Server

Edit `Saved/WowCredentials.json`:

```json
{
  "ServerAddress": "your.azerothcore.server:8085",
  "AccountName": "youraccountname",
  "Password": "yourpassword"
}
```

### 5. Run

```bash
# Launch with login screen
./run_game.sh

# Auto-login with saved credentials
./run_game.sh --autologin

# Model viewer (orbit camera)
./Scripts/run_model_viewer.sh

# World map viewer
./Scripts/run_world.sh
```

---

## Project Structure

```
WowUnreal/
├── Source/
│   ├── WowData/              # Format parsers (ADT, M2, WMO, BLP, DBC, MPQ)
│   │   ├── Public/
│   │   └── Private/
│   ├── WowAssets/            # Asset conversion pipeline (BLP→Texture, M2→SkeletalMesh)
│   │   ├── Public/
│   │   └── Private/
│   ├── WowWorld/             # World streaming, terrain, sky, water, audio
│   │   ├── Public/
│   │   └── Private/
│   ├── WowUI/                # Lua VM, FrameXML, addon loader, UI widgets
│   │   ├── Public/
│   │   └── Private/
│   ├── WowNetwork/           # Auth socket (SRP6), world socket (ARC4), opcodes
│   │   ├── Public/
│   │   └── Private/
│   ├── WowClient/            # Login flow, credentials, settings
│   │   ├── Public/
│   │   └── Private/
│   ├── WowUnreal/            # Game shell, gameplay controller, player character
│   │   ├── Public/
│   │   └── Private/
│   ├── WowTests/             # Unit tests
│   │   └── Private/
│   └── ThirdParty/
│       ├── StormLib/         # MPQ library
│       ├── Lua/              # Lua 5.1.5
│       └── pugixml/          # XML parser
├── Content/
│   ├── Characters/           # Character materials and test assets
│   ├── World/                # World test maps
│   └── UI/                   # Login screen assets
├── Config/
│   ├── DefaultEngine.ini     # Engine settings
│   ├── DefaultGame.ini       # Game settings
│   └── DefaultInput.ini      # Input mappings
├── Saved/
│   ├── WowCredentials.json   # Server and account config
│   └── Logs/                 # Engine logs
├── Scripts/
│   ├── run_world.sh          # Launch world viewer
│   ├── run_model_viewer.sh   # Launch model viewer
│   └── run_map.sh            # Launch specific test map
├── specs/                    # Technical specifications per subsystem
│   ├── overview.md           # Architecture and goals
│   ├── terrain-lod.md        # Terrain LOD system
│   ├── character.md          # Character rendering
│   ├── networking.md         # Network protocol
│   ├── lua-api.md            # Lua API bindings
│   ├── ui-framexml.md        # FrameXML UI system
│   └── ...                   # (14 spec files total)
├── build.sh                  # Build script
├── run_game.sh               # Launch game
├── SPEC.md                   # Full project specification
├── IMPLEMENTATION_PLAN.md    # Feature checklist with status
├── .gitignore
├── README.md                 # This file
└── WowUnreal.uproject        # Unreal Engine project file
```

---

## Configuration

### Server Connection

Edit `Saved/WowCredentials.json`:

```json
{
  "ServerAddress": "localhost:8085",
  "AccountName": "test",
  "Password": "test",
  "Realm": "Trinity Core"
}
```

Replace with your AzerothCore server address and credentials.

### Engine Settings

Modify `Config/DefaultEngine.ini` for:
- Resolution and quality settings
- View distance and LOD thresholds
- Memory limits for streaming
- Network timeout and compression settings

### Input Mappings

See `Config/DefaultInput.ini` for keyboard, mouse, and gamepad bindings.

---

## Development

### Building from Source

**macOS:**

```bash
./build.sh
```

**Windows:**

```bash
call "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" WowUnreal Win64 Development ^
  -project="%cd%\WowUnreal.uproject" -waitmutex
```

### Running Tests

```bash
./build.sh -test
```

Tests validate:
- File format parsing (ADT, M2, WMO, BLP, DBC)
- Asset conversion accuracy
- Skeletal mesh bone hierarchy
- Texture streaming and decompression

### Code Organization

Each module follows standard Unreal Engine layout:
- `Public/` — Public headers (API surface)
- `Private/` — Implementation files and private headers
- `[ModuleName].Build.cs` — Module build configuration

### Adding New Features

1. Identify the relevant module (see Architecture section)
2. Review `specs/` directory for protocol and format documentation
3. Add code to appropriate `Private/` folder
4. Expose APIs via `Public/` headers if needed
5. Add unit tests to `WowTests/`
6. Update documentation in `specs/`

### Key Files to Know

- `Source/WowNetwork/Private/WowPacketHandler.cpp` — Opcode dispatch and packet handlers
- `Source/WowNetwork/Private/Net/WowAuthSocket.cpp` — SRP6 auth handshake
- `Source/WowAssets/Private/WowSkeletalMeshBuilder.cpp` — M2 → USkeletalMesh conversion
- `Source/WowUI/Private/WowLuaVM.cpp` — Lua 5.1.5 VM integration
- `Source/WowUI/Private/WowFrameXmlParser.cpp` — FrameXML parsing
- `Source/WowWorld/Private/WowWorldManager.cpp` — World streaming coordinator
- `Source/WowWorld/Private/WowTerrainTile.cpp` — ADT terrain tile rendering
- `Source/WowUnreal/Private/WowGameplayController.cpp` — Main gameplay logic

---

## Status

WowUnreal is under active development. This table shows the current state of major systems:

| System | Status | Notes |
|--------|--------|-------|
| Data Parsing | Done | All WoW 3.3.5a formats: MPQ, BLP, ADT, WDT, WDL, M2, WMO, DBC (32 typed wrappers) |
| Terrain Rendering | Done | 4-layer splatting, 3-level LOD, async streaming |
| Character Models | Done | All 10 races x 2 genders, composite textures, equipment, 150+ animations |
| WMO Buildings | Done | Per-group meshes, materials, doodad sets |
| Doodads | Done | Instanced rendering, distance-based streaming |
| Sky/Atmosphere | Done | Light.dbc zone blending, day/night, sun/moon, fog, clouds |
| Water | Done | MH2O, ocean plane, WMO liquid |
| Audio | Partial | Zone music (MP3) and ambience (WAV) working; spell/UI sounds pending |
| Network Protocol | Done | 121+ opcodes, SRP6 auth, ARC4 encryption, Warden handler |
| Login Flow | Done | Themed login screen, realm select, character select/create, loading screens |
| Movement | Done | WASD, jump, swim, auto-run, 3rd-person camera; flying/mounts pending |
| Combat | Done | Auto-attack, spell casting, cast bar, targeting, floating damage text |
| UI System | Done | 5200+ frames from FrameXML, ~375 Lua APIs, addon loading |
| Chat | Done | Say, Yell, Party, Guild, Whisper, channels |
| Quests | Done | Quest log, dialog, accept/complete/rewards |
| Inventory | Done | Bags, loot, vendor, item tooltips; drag-drop pending |
| Social | Done | Friends, guild roster, party/group system |
| Minimap | Partial | Rendering, zoom, zone name; NPC dots and tracking pending |
| Spell Effects | Partial | Basic spell missiles; full visual effect chain pending |
| Mail/Auction/Trade | Not Started | Server opcodes exist but no UI |
| Dungeon/Raid UI | Not Started | Instance maps, LFD, raid frames pending |
| PvP | Not Started | Battlegrounds, arenas, honor system |
| Settings UI | Not Started | Video, audio, keybinding menus |

### Known Limitations

- macOS is the primary platform; Windows builds are planned but untested
- Flying mounts and transport (boats/zeppelins) not yet implemented
- Some advanced spell visual effects use placeholder rendering
- Addon compatibility covers most base UI; complex addons may hit unimplemented API stubs
- No dungeon/raid-specific features yet

---

## Contributing

WowUnreal welcomes contributions from developers, researchers, and enthusiasts. Whether you're fixing bugs, adding features, or improving documentation, your help is valued.

### Getting Started

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature-name`
3. Make changes and test thoroughly
4. Commit with clear, descriptive messages
5. Push to your fork and open a pull request

### Code Style

- Follow Unreal Engine coding standards (PascalCase, `F` prefix for structs, `U`/`A` prefix for UObject/AActor)
- Use meaningful variable names
- Add comments for complex logic
- Write tests for new functionality

### Documentation

- Update relevant `.md` files in `specs/` for protocol or format changes
- Include code comments for non-obvious implementation details
- Add examples for new APIs in documentation

### Testing

Before submitting a PR:
- Build the project successfully on your platform
- Run the test suite
- Verify the game launches and doesn't crash
- Test new features manually with `-autologin` flag
- Check that your changes don't break existing functionality

### Reporting Issues

When reporting bugs:
1. Describe the issue and reproduction steps
2. Include relevant logs from `Saved/Logs/`
3. Note your platform (macOS/Windows), Unreal Engine version, and WoW client version
4. Attach screenshots if relevant

### Architecture and Design

For architectural questions or large features, review:
- `specs/overview.md` — Architecture and project goals
- `specs/networking.md` — Network protocol details
- `specs/` directory — 14 spec files covering each subsystem
- `SPEC.md` — Full project specification
- `IMPLEMENTATION_PLAN.md` — Feature checklist with completion status

---

## Legal

**WowUnreal does NOT ship any Blizzard Entertainment assets.** All WoW game data (textures, models, sounds, databases) must be supplied by the user from their own World of Warcraft 3.3.5a installation.

This is an **educational and research project** demonstrating:
- Game engine architecture (Unreal Engine 5.7)
- Network protocol reverse-engineering and implementation
- Binary file format parsing and asset conversion
- Scripting engine integration (Lua 5.1.5)

**World of Warcraft** is a trademark of Blizzard Entertainment, Inc. This project is not affiliated with, endorsed by, or associated with Blizzard Entertainment.

### License

See [LICENSE](LICENSE) for details.

---

## Support

For questions, issues, or discussions:
- Open an issue on GitHub
- Check `specs/` for technical documentation
- Review the source code in relevant modules
- Consult `Saved/Logs/` for runtime debugging information

---

**Happy hacking!**
