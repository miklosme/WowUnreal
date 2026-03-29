# WoW 3.3.5 Unreal Engine Client — Implementation Plan

## Overview
Build a performant WoW 3.3.5a client in UE 5.7 that reads original MPQ data files, renders zones, supports the full native WoW UI (Lua + XML + addons), and connects to AzerothCore servers.

## Architecture
8 modules: WowUnreal (game shell), WowData (format parsers), WowAssets (UE conversion), WowWorld (streaming/rendering), WowUI (Lua/XML frames), WowNetwork (auth/world protocol), WowClient (convenience features), WowTests (testing utilities).

## Test Server
- Remote: 192.168.1.5 (Auth: 3724, World: 8085)
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
- [x] Long-lived loading screen Slate texture/brush lifetime stability (retain background resources while visible, release them before rebuilding the loading screen)
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

## Recently Merged (from parallel agents, 2026-03-15)

- [x] **Lua API expansion**: ~50 → 150+ functions (string, table, math, unit, spell, action bar, chat, inventory, quest, social, CVar)
- [x] **Warden anti-cheat**: responds to all 6 packet types (module, hash, memory, page, MPQ, timing checks)
- [x] **Teleport handling**: MSG_MOVE_TELEPORT_ACK, SMSG_TRANSFER_PENDING, SMSG_NEW_WORLD with worldport ACK
- [x] **NPC waypoint movement**: SMSG_MONSTER_MOVE spline parsing + linear interpolation in tick
- [x] **Nameplates**: floating name + health bar above entities (UWidgetComponent, color-coded)
- [x] **Combat log**: scrollable color-coded widget (spells yellow, damage red, heals green)
- [x] **Loading screen images**: MapDbc → LoadingScreens.dbc → BLP background
- [x] **Death screen**: full overlay with "Release Spirit", health monitoring, CMSG_REPOP_REQUEST
- [x] **WoW cursors**: loads BLP from Interface/Cursor/ (Point, Attack, Speak, etc.)
- [x] **Tooltip system**: hover detection, entity name/level/health with WoW styling
- [x] **M2 particle effects**: basic point light + billboard for fire/smoke emitters

---

## In Progress / Needs Building

### P1: Fully Playable Character — Animation & Combat
Goal: Walk around, fight mobs, cast spells, see other players doing the same.

**Animation System (local player + networked players + NPCs):**
- [x] Animation state machine: idle, walk, run, jump, fall, swim, death, sit, sleep, kneel
- [x] Combat animations: attack (1H, 2H, bow, unarmed), parry, dodge, block
- [x] Spell cast animations: cast, channel, omni-cast (instant)
- [x] Emote animations: dance, wave, cheer, laugh, cry, etc. (AnimationData.dbc mapping)
- [x] Wire SMSG_EMOTE and SMSG_TEXT_EMOTE to animation playback
- [x] Map M2 animation IDs to UAnimSequence per-race/gender model
- [x] Play correct animation based on entity movement flags (walking, running, falling, swimming)
- [x] Networked player animations: read movement flags from UPDATE_OBJECT, play matching anims

**Local Player Character Model:**
- [x] Spawn local player character model using race/gender from SMSG_CHAR_ENUM
- [x] Attach to AWowPlayerCharacter pawn (skeletal mesh on the character)
- [x] Apply composite texture (skin + face + hair from customization)
- [x] Equipment rendering on local player (weapons, armor, helmet, cape from entity fields)
- [x] Update equipment when UNIT_FIELD changes (equip/unequip live)

**Networked Player Models:**
- [x] Spawn other player models by race/gender from UNIT_BYTES_0
- [x] Apply equipment from UPDATE_OBJECT equipment fields
- [x] Name query: send CMSG_NAME_QUERY for unknown player GUIDs, cache responses
- [x] Show player name on nameplate (from name query response)

**NPC/Creature Models:**
- [x] Spawn NPC by DisplayId (already works)
- [x] Name query: send CMSG_CREATURE_QUERY, cache creature name/title
- [x] Show NPC name + title on nameplate
- [x] NPC scale from CreatureDisplayInfo.dbc

**Combat — Attacking & Spells:**
- [x] Right-click to auto-attack targeted enemy (CMSG_ATTACKSWING)
- [x] Stop auto-attack (CMSG_ATTACKSTOP)
- [x] Cast spell by ID (CMSG_CAST_SPELL) — wire to action bar clicks
- [x] Show cast bar during spell casting (from SMSG_SPELL_START CastTime)
- [x] Show spell impact effects (basic: flash at target location)
- [x] Process SMSG_ATTACKERSTATEUPDATE for damage numbers
- [x] Floating combat text (damage/heal numbers above entities)

### P2: Action Bar & Spell Book UI
- [x] Action bar widget (12 slots × main bar + bonus bars)
- [x] Drag spells from spell book to action bar
- [x] Click action bar slot to cast spell
- [x] Cooldown sweep animation on action bar icons
- [x] Spell book UI (tabs per spell school)
- [x] Talent tree UI (read from SMSG_TALENTS_INFO)

### P3: Core Gameplay Systems
- [x] Taxi/flight paths (SMSG_SHOWTAXINODES, taxi map UI, auto-fly along path)
- [x] Loot window (SMSG_LOOT_RESPONSE → show loot UI → CMSG_AUTOSTORE_LOOT_ITEM)
- [x] Vendor/merchant window (SMSG_LIST_INVENTORY)
- [x] Quest dialog (SMSG_QUESTGIVER_QUEST_DETAILS, accept/decline/complete)
- [x] Bank (SMSG_SHOW_BANK)
- [x] Mail system (SMSG_MAIL_LIST_RESULT)
- [x] Trade window (SMSG_TRADE_STATUS)
- [x] Group/party UI (invite, accept, leave, ready check)
- [x] Raid UI (raid frames, marks, ready check)
- [x] Duel system (SMSG_DUEL_REQUESTED)
- [ ] Pet/companion system (pet bar, pet actions)

### P4: Bag & Inventory
- [x] Bag UI (16-slot backpack + 4 equipped bags)
- [x] Item icons from ItemDisplayInfo.dbc → BLP
- [x] Item tooltips (name, stats, flavor text from item cache)
- [ ] Equip/unequip items (CMSG_AUTOEQUIP_ITEM)
- [x] Item quality colors (poor/common/uncommon/rare/epic/legendary)
- [ ] Stack splitting, item deletion

### P5: Chat System
- [x] Chat input box with channel switching (/s /p /g /w /y /1 /2)
- [x] Chat window with tabs (General, Combat Log, Trade, etc.)
- [x] Whisper support (CMSG_MESSAGECHAT type WHISPER)
- [x] Channel join/leave (CMSG_JOIN_CHANNEL, CMSG_LEAVE_CHANNEL)
- [ ] Chat link clicking (items, spells, achievements)

### P6: Minimap
- [x] Circular minimap rendering from terrain height data
- [x] Player arrow (direction indicator)
- [ ] Other player/NPC dots
- [x] Zone name display
- [x] Minimap zoom
- [ ] Tracking icons (herbs, mining, etc. from SMSG_UPDATE_OBJECT flags)

### P7: Effects & Polish
- [ ] M2 ribbon emitters (weapon trails)
- [ ] M2 light emitters (lanterns, spell glow)
- [ ] WMO interior lighting from Light.dbc
- [ ] Spell visuals (SpellVisual → SpellVisualKit → effect attachment chain)
- [ ] Custom terrain shader (reduce 256 draw calls/tile)
- [ ] Shadow flickering fix on WMOs

### P8: Full WoW UI Boot
- [x] Load real Blizzard FrameXML from MPQ (action bars, minimap, chat, unit frames, buffs, bags)
- [x] Full addon loading from Interface/AddOns/
- [x] Font rendering with WoW .ttf fonts (in progress — P7 agent)
- [x] FrameXML child `OnLoad` bootstrap stability (preserve frame Lua objects across creation/compile, seed parent IDs before child `OnLoad`, keep RuneFrame bootstrap alive without reintroducing Lua heap blowups)
- [x] Secure attribute delegate dispatch for Blizzard dropdown bootstrap (`SetAttribute` now drives `OnAttributeChanged` so `UIDROPDOWNMENU_INIT_MENU`/unit popup dropdowns initialize)
- [x] Unit relationship API parity for Blizzard unit popup bootstrap (`UnitCanCooperate` and shared friend/hostile resolution for party/target dropdown setup)
- [x] UnitPopup utility API coverage for RAF/loot/difficulty gates (`IsReferAFriendLinked`, `GetOptOutOfLoot`, `HasLFGRestrictions`, `GetInstanceInfo`, `CanChangePlayerDifficulty`)
- [x] RAF summon helper API coverage for UnitPopup/FriendsFrame (`GetSummonFriendCooldown`, `CanSummonFriend`, `CanGrantLevel`, `SummonFriend`, `GrantLevel`)
- [x] Video options dropdown API shape parity (`GetMultisampleFormats`, `GetRefreshRates`, and no-op setters return the types Blizzard expects)
- [x] FrameXML animation-object global exposure for tutorial callouts (named `<Animations>` descendants resolve `$parent`, register in Lua, and support `Play`/`Stop`)
- [x] Frame hover hit-testing parity for `IsMouseOver` (Lua method routes through cached frame rects and cursor position with WoW-style top/bottom/left/right offsets)
- [x] UIParent arena layout stub parity (`GetNumArenaOpponents` returns a safe non-arena default for watch-frame placement)
- [x] Scoped XML `OnLoad function="..."` parity for LFR browse bootstrap (`LFRBrowseFrame_OnLoad` is allowed without reopening broad handler compilation)
- [ ] Character creation 3D preview with customization sliders

### P9: 3D Login Screen Backgrounds
- [ ] Classic: Dark Portal (WMO + effects)
- [ ] Burning Crusade: Outland portal
- [ ] Wrath of the Lich King: Sindragosa/Icecrown
- [ ] Slow-orbit camera + login music

---

## Test Infrastructure
- `./run_game.sh` — Login screen (credentials prefilled)
- `./run_game.sh --autologin` — Auto-login with first character
- `./run_game.sh --autologin --createchar` — Create new char + enter world
- `./run_game.sh --build` — Build first, then launch
- `./Scripts/run_map.sh <MapName>` — Launch specific test map
- `./Scripts/run_model_viewer.sh` — Model viewer with orbit camera
- Network test: `-testscene=network` (headless E2E)
