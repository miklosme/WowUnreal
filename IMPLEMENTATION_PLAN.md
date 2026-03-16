# WoW 3.3.5 Unreal Engine Client — Implementation Plan

## Overview
Build a performant WoW 3.3.5a client in UE 5.7 that reads original MPQ data files, renders zones, supports the full native WoW UI (Lua + XML + addons), and connects to AzerothCore servers.

## Architecture
7 modules: WowUnreal (game shell), WowData (format parsers), WowAssets (UE conversion), WowWorld (streaming/rendering), WowUI (Lua/XML frames), WowNetwork (auth/world protocol), WowClient (convenience features).

## Test Server
- Remote: 127.0.0.1 (Auth: 3724, World: 8085)
- Account: WowTestUser / WowTestPass
- Characters: Testhumanm (Level 3, tile 29,51), Northshire (Level 1, tile 32,48), Stormheart (Level 1, tile 32,48)

## Reference Projects (~/projects/)
- noggit3: ADT/WMO/M2 struct definitions
- pywowlib: Python format parsers
- wowmodelviewer: M2 rendering pipeline
- azerothcore-wotlk: Network protocol, SRP6, opcodes

---

## Completed (Verified Working)

### Core Systems
- [x] MPQ reading (StormLib, 18 archives)
- [x] BLP texture parsing (DXT passthrough, paletted)
- [x] DBC parsing (21 typed wrappers)
- [x] ADT/WDT/WDL terrain parsing
- [x] M2 model parsing (vertices, bones, animations, skins)
- [x] WMO parsing (root + groups)
- [x] Coordinate conversion (WoW ↔ ADT ↔ UE)

### Terrain Rendering
- [x] Terrain mesh building (256 chunks/tile, splatmap textures)
- [x] Async tile streaming (camera-based load/unload with LOD)
- [x] WDL distant terrain, LOD1 mid-distance meshes
- [x] Runtime Virtual Textures
- [x] Terrain collision (complex-as-simple)
- [x] Water rendering (MH2O, ocean plane, WMO liquid)
- [x] Nanite for WMO static meshes

### World Objects
- [x] M2 doodad spawning (instanced rendering)
- [x] WMO building rendering (per-group meshes, full rotation)
- [x] Distance-based object streaming

### Sky & Atmosphere
- [x] Time-of-day sky with Light.dbc zone blending
- [x] Sun/moon disc billboards
- [x] Fog from LightFloatParams.dbc
- [x] Cloud layer

### Character Rendering
- [x] M2 → USkeletalMesh with bone weights
- [x] Composite character textures (skin + face + hair + underwear from CharSections.dbc)
- [x] Hair split into separate skeletal mesh
- [x] Geoset visibility (hair/facial hair/equipment)
- [x] Equipment attachment via ItemDisplayInfo.dbc
- [x] All 10 races × 2 genders
- [x] Animation parsing (M2 → UAnimSequence)
- [x] Creature spawning by DisplayId

### Networking (121 opcodes)
- [x] SRP6 auth + ARC4 encryption
- [x] Realm selection, character enum/create/delete
- [x] SMSG_CHAR_ENUM parsing (fixed firstLogin byte)
- [x] Player login + SMSG_LOGIN_VERIFY_WORLD
- [x] Entity system (UPDATE_OBJECT, typed hierarchy)
- [x] Movement sync (heartbeat, server correction)
- [x] Chat, spells, combat, quest, talent, social, inventory handlers
- [x] Keep-alive, time sync

### Login & World Entry Flow
- [x] WoW-themed login screen with expansion tabs (Classic/BC/WotLK)
- [x] Themed colors per expansion (gold/fel green/icy blue)
- [x] Credential prefill from WowCredentials.json
- [x] Realm select screen (WoW-styled)
- [x] Character select screen with class-colored names
- [x] Character creation screen with race/class/gender selectors
- [x] 3D character preview in character select (SceneCapture2D)
- [x] Deferred terrain loading (no world behind login screen)
- [x] Loading screen with tile progress
- [x] Ground snap after terrain loads (line trace to terrain surface)
- [x] `-autologin` flag (prefills and auto-submits through UI)
- [x] `-createchar` flag (auto-creates Human Mage, enters world)
- [x] Single-instance lockfile (prevents dual UE launches)

### Player Controller
- [x] 3rd-person chase camera (spring arm, zoom, orbit)
- [x] WASD movement, mouse look, jump, walk/run toggle, auto-run
- [x] Swim speed, fall damage tracking
- [x] Left-click targeting (CMSG_SET_SELECTION)
- [x] Entity model spawning from live server data

### Audio
- [x] Zone music from MPQ (MP3 with A/B crossfading)
- [x] Ambient sounds from MPQ (WAV)
- [x] Zone detection from MCNK area IDs

### WoW UI System (Lua + FrameXML)
- [x] Lua 5.1 VM (sandboxed, memory limited 128MB, instruction limited)
- [x] FrameXML parser (19 frame types, anchors, strata, templates)
- [x] Addon/TOC loader with dependency resolution (Kahn's algorithm)
- [x] SavedVariables persistence
- [x] Event system (OnEvent, OnLoad, OnClick, OnUpdate dispatch)
- [x] ~50 Lua API functions implemented
- [x] Frame methods (SetPoint, SetSize, Show/Hide, SetAlpha, etc.)
- [x] UMG widget integration

---

## In Progress / Needs Fixing

### P1: Expand Lua API Coverage
Currently ~50/1200+ functions. Needed for WoW UI to actually render:
- [ ] Font loading (.ttf from MPQ → UFont)
- [ ] Texture loading for UI frames (BLP → UTexture for frames)
- [ ] Complete unit API (UnitBuff, UnitDebuff, UnitAura, etc.)
- [ ] Action bar API (GetActionInfo, UseAction, etc.)
- [ ] Bag/inventory API
- [ ] Spell book API
- [ ] Quest log API
- [ ] Achievement/statistics API
- [ ] Map/minimap API
- [ ] Social/friends API
- [ ] Guild API
- [ ] CVar system (GetCVar, SetCVar)

### P2: Visual Gameplay Features
- [ ] Minimap rendering from terrain data
- [ ] Nameplates (name + health bar above entities)
- [ ] Combat log
- [ ] Loading screen images from LoadingScreens.dbc
- [ ] WoW mouse cursor textures
- [ ] Tooltip system (items, spells, NPCs)

### P3: Gameplay Systems
- [ ] Warden anti-cheat responses (will get kicked without)
- [ ] Teleport handling (MSG_MOVE_TELEPORT_ACK)
- [ ] Death/corpse run
- [ ] Taxi/flight paths
- [ ] Bank/mail/auction/trade
- [ ] Battleground/arena
- [ ] Pet system
- [ ] Duel system
- [ ] Group/raid UI

### P4: NPC/Creature Behavior
- [ ] NPC movement from SMSG_MONSTER_MOVE waypoint packets
- [ ] Creature idle/walk/run animation states
- [ ] Player movement interpolation (smooth network sync)

### P5: Effects & Polish
- [ ] M2 particle emitters → Niagara systems
- [ ] Fire/smoke effects (campfires, torches)
- [ ] M2 ribbon emitters (weapon trails)
- [ ] M2 light emitters (lanterns, spell glow)
- [ ] WMO interior lighting from Light.dbc
- [ ] Spell visuals (SpellVisual → SpellVisualKit chain)

### P6: Terrain Polish
- [ ] Custom terrain shader (reduce 256 draw calls/tile)
- [ ] Shadow flickering fix on WMOs
- [ ] Distance-based UV scaling

### P7: Full WoW UI Boot
- [ ] Load real Blizzard FrameXML from MPQ (action bars, minimap, chat, unit frames, buffs, bags)
- [ ] Full addon loading from Interface/AddOns/
- [ ] Font rendering with WoW .ttf fonts
- [ ] Character creation 3D preview with customization sliders

### P8: 3D Login Screen Backgrounds
Render authentic WoW login backgrounds using M2/WMO models instead of video:
- [ ] Classic: Dark Portal scene (WMO from World/wmo/Azeroth/DarkPortal/ + effects)
- [ ] Burning Crusade: Outland portal scene
- [ ] Wrath of the Lich King: Sindragosa/Icecrown scene (creature/frostwyrm M2)
- [ ] Slow-orbit camera animation per expansion
- [ ] Login music per expansion from MPQ

---

## Test Infrastructure
- `./run_game.sh` — Login screen (credentials prefilled)
- `./run_game.sh --autologin` — Auto-login with first character
- `./run_game.sh --autologin --createchar` — Create new char + enter world
- `./run_game.sh --build` — Build first, then launch
- `./Scripts/run_map.sh <MapName>` — Launch specific test map
- `./Scripts/run_model_viewer.sh` — Model viewer with orbit camera
- Network test: `-testscene=network` (headless E2E)
