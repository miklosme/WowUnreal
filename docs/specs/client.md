# WowUnreal — WoW 3.3.5a Client Specification

## Overview

The intended product is a playable World of Warcraft 3.3.5a (Wrath of the Lich King) client that reads original MPQ data, connects to AzerothCore, and supports Lua/FrameXML addons. The fork is migrating the original UE 5.7 project to Unreal Engine 5.8.1.

**Design Priorities:**
1. **Visual Quality** — Leverage UE5's rendering (Lumen, Nanite where applicable, modern materials) to make the original WoW world look stunning
2. **High Performance** — 60+ FPS target on mid-range hardware, efficient streaming, minimal hitching
3. **Full Gameplay** — Complete client supporting combat, questing, dungeons, raids, PvP, auction house, mail, guilds — everything a player expects
4. **Addon Compatibility** — Lua 5.1 + FrameXML system for WoW addon support

**Target Server:** AzerothCore 3.3.5a (protocol version 12340)

### Data Requirement

This client does **not** ship any Blizzard assets. It reads directly from an existing World of Warcraft 3.3.5a installation's MPQ data files at runtime. All textures, models, terrain, audio, UI XML/Lua, and database files (DBC) are loaded from the original game data.

Supply the data path at runtime with `-wowdata="/absolute/path/to/Data"`. See [game-data setup](../setup/game-data.md).

The MPQ archive chain loaded (in priority order):
```
patch-3.MPQ → patch-2.MPQ → patch.MPQ → lichking.MPQ →
expansion.MPQ → common-2.MPQ → common.MPQ → base MPQs
+ locale-specific: enUS/patch-enUS-3.MPQ → ... → enUS/locale-enUS.MPQ
```

Users must supply their own copy of the WoW 3.3.5a client data. The current reliable configuration surface is the `-wowdata="<path>"` launch argument.

---

## Architecture

### Module Structure

```
WowUnreal/
├── WowData        — Binary format parsers (ADT, WDT, WDL, M2, WMO, BLP, DBC, MPQ)
├── WowAssets      — Asset conversion pipeline (BLP→Texture, M2→SkeletalMesh, etc.)
├── WowWorld       — World streaming, terrain, sky, water, audio, doodads, WMOs
├── WowUI          — Lua VM, FrameXML, widgets, addon loader
├── WowNetwork     — Auth/world sockets, packet handlers, entity system
├── WowClient      — Login flow, credentials, settings
├── WowUnreal      — Game shell, gameplay controller, player character, UI widgets
├── WowTests       — Unit tests
└── ThirdParty     — StormLib, Lua 5.1.5, pugixml
```

### Current Status (What Exists)

| Component | Status | Notes |
|-----------|--------|-------|
| MPQ reading | Done | StormLib integration, archive chain loading |
| BLP parsing | Done | DXT1/3/5 passthrough, paletted support |
| ADT parsing | Done | Heights, normals, layers, alpha maps, MDDF/MODF |
| WDT parsing | Done | Tile existence grid |
| M2 parsing | Done | Vertices, indices, textures, render passes, bones |
| WMO parsing | Done | Root + groups, materials, batches |
| DBC parsing | Done | 32 typed wrappers implemented |
| Terrain rendering | Done | LOD system (3 levels), async loading, streaming |
| Water rendering | Done | MH2O, ocean plane, WMO liquid |
| Sky system | Done | Light.dbc interpolation, skydome, sun/moon, clouds, fog, day/night |
| World streaming | Done | Camera-based tile load/unload, progress screens |
| Doodad spawning | Done | UStaticMesh instanced rendering |
| WMO spawning | Done | Group-based rendering with portal framework |
| Auth networking | Done | SRP6, ARC4-drop1024, realm list |
| World socket | Done | Encrypted packet framing, 121+ opcode handlers |
| Lua VM | Done | Lua 5.1.5, 375+ API functions, memory limit, timeout, error handling |
| Frame XML | Done | 19 frame types, templates, anchoring, strata |
| Addon loader | Done | TOC parser, dependency resolution, SavedVariables, Blizzard UI loading |

---

## Phase 1: Core Data Pipeline (DONE)

### 1.1 MPQ Archive System
- [x] Load archive chain: patch-3 → patch-2 → patch → lichking → expansion → common → common-2 → base MPQs
- [x] Locale-aware loading (enUS, etc.)
- [x] Thread-safe file extraction
- [x] File path normalization (backslash → forward slash, case-insensitive)

### 1.2 Texture Pipeline (BLP → UTexture2D)
- [x] BLP format parsing (type 1: paletted, type 2: DXT compressed)
- [x] DXT1/DXT3/DXT5 → UE5 compressed texture (GPU-native, no CPU decompression)
- [x] Paletted → BGRA8 conversion
- [x] Mipmap chain support
- [x] Texture cache with weak references

### 1.3 DBC Database System
- [x] Generic DBC record reader
- [x] Typed DBC wrappers — 32 typed wrappers implemented:
  - `Map.dbc` — Map IDs, names, instance types
  - `AreaTable.dbc` — Zone/subzone definitions
  - `Light.dbc` + `LightParams.dbc` + `LightIntParams.dbc` — Outdoor lighting
  - `LiquidType.dbc` — Water/lava/slime definitions
  - `SpellVisual.dbc` + `SpellVisualKit.dbc` — Spell effect lookups
  - `CreatureDisplayInfo.dbc` + `CreatureModelData.dbc` — NPC models
  - `ItemDisplayInfo.dbc` — Item visuals
  - `CharSections.dbc` — Character customization
  - `ChrRaces.dbc` — Race data
  - `AnimationData.dbc` — Animation IDs and flags
  - `SoundEntries.dbc` — Sound file references
  - `LoadingScreens.dbc` — Loading screen textures
  - `GroundEffectTexture.dbc` + `GroundEffectDoodad.dbc` — Ground clutter
  - `EmotesText.dbc` + `EmotesTextData.dbc` — Emote system
  - `Talent.dbc` + `TalentTab.dbc` — Talent trees
  - `Spell.dbc` — Spell data (the big one: 234 fields per record)
  - And 15+ more critical tables

---

## Phase 2: Terrain & World Streaming

### 2.1 Terrain Rendering
- [x] ADT chunk mesh generation (16x16 chunks per tile, 9x9 + 8x8 vertex grids)
- [x] Height map application
- [x] Per-chunk normals
- [x] 4-layer texture splatting with 3 alpha maps
- [x] Terrain LOD system (3 levels working)
- [ ] **TODO:** Vertex colors (MCCV chunks) for terrain tinting
- [ ] **TODO:** Shadow maps (MCSH chunks)
- [ ] **TODO:** Ground clutter (grass/flowers from GroundEffectTexture.dbc)
- [ ] **TODO:** Instance map support (interior-only maps with no ADT terrain)

### 2.2 World Streaming
- [x] Camera-based tile loading with hysteresis
- [x] WDT tile existence checks
- [x] Distance-based object streaming (doodads + WMOs)
- [x] Async loading on background threads
- [x] Loading screen with progress bar during transitions
- [ ] **TODO:** Hybrid streaming — camera-based in viewer mode, player-position-based in game mode
- [ ] **TODO:** Memory budget enforcement (target: 2GB for world data)

### 2.3 Water Rendering
- [x] MH2O chunk parsing (liquid heights, types, flags)
- [x] Water material with animated UV scrolling, depth-based transparency, fresnel reflections
- [x] Ocean plane for deep water areas
- [x] WMO liquid support (indoor water, fountains)
- [ ] **TODO:** Caustics on submerged terrain
- [ ] **TODO:** Lava material (emissive, animated, no transparency)
- [ ] **TODO:** Slime material (opaque green, animated)

### 2.4 Sky & Atmosphere
- [x] Light.dbc → sky color/fog interpolation based on time of day and zone
- [x] Skydome with gradient colors (top, middle, horizon bands)
- [x] Sun/moon positions and rotation
- [x] Cloud layers (scrolling textures)
- [x] Distance fog matching sky horizon color
- [x] Day/night cycle with smooth transitions
- [ ] **TODO:** Weather state integration with sky (rain, snow, sandstorm)

---

## Phase 3: Model Rendering

### 3.1 M2 Static Models (Doodads)
- [x] Vertex/index buffer extraction
- [x] Texture assignment per render pass
- [x] Basic material creation
- [x] Migrated to UStaticMesh with instanced rendering
- [ ] **TODO:** Material blending modes (opaque, alpha test, alpha blend, additive)
- [ ] **TODO:** Backface culling flags per render pass
- [ ] **TODO:** Collision generation for interactive doodads

### 3.2 M2 Animated Models (Characters, Creatures, Spells)
- [x] Bone hierarchy → UE5 Skeleton asset
- [x] Animation sequences (Stand, Walk, Run, Attack, Cast, Death, etc.)
- [x] SkeletalMesh pipeline with bone weights and transforms
- [x] Attachment points (helm, shoulders, weapons, effects)
- [ ] **TODO:** Particle emitters (from M2 particle data)
  - Billboard particles
  - Ribbon trails (weapon enchants, spell effects)
- [ ] **TODO:** Texture animations (UV scrolling, transform tracks)

### 3.3 Character Rendering
- [x] Race/gender model loading from `CreatureDisplayInfo.dbc`
- [x] Character customization compositing (skin color, face, hair style, etc.)
- [x] Equipment rendering (armor pieces, weapons, tabards)
- [ ] **TODO:** Mount models with rider attachment
- [ ] **TODO:** Morph/transform effects (druid forms, polymorph, etc.)

### 3.4 WMO Buildings
- [x] Group mesh rendering
- [x] Material assignment
- [x] Portal-based visibility culling framework
- [x] WMO doodad sets (furniture, decorations inside buildings)
- [x] WMO liquid (indoor water features)
- [ ] **TODO:** Interior/exterior group flags optimization
- [ ] **TODO:** WMO lighting (MOLT chunks — colored point lights)

### 3.5 Visual Effects
- [ ] **TODO:** Spell visual system:
  - `SpellVisual.dbc` → `SpellVisualKit.dbc` → effect chains
  - Cast effects, projectiles, impact effects, persistent area effects
  - Niagara particle system integration
- [ ] **TODO:** Environment effects:
  - Weather: rain, snow, sandstorm (particles + fog + sound)
  - Torch/campfire glow (point lights + particles)
- [ ] **TODO:** Post-processing:
  - Death effect (desaturation)
  - Underwater tint
  - Drunk effect (blur + sway)
  - Ghost effect (transparency + blue tint)

---

## Phase 4: Networking & Server Communication

### 4.1 Authentication
- [x] SRP6 challenge/proof
- [x] ARC4-drop1024 encryption setup
- [x] Realm list retrieval
- [ ] **TODO:** Reconnect handling
- [ ] **TODO:** Transfer/redirect handling

### 4.2 World Connection
- [x] Encrypted world socket
- [x] Packet framing (opcode + size)
- [x] 121+ opcode handlers implemented
- [ ] **TODO:** Packet decompression (zlib for some large packets)
- [ ] **TODO:** Latency measurement and display

### 4.3 Core Packet Handlers

All handlers implemented — 121+ opcodes fully working.

**Login & Character Select:**
- [x] SMSG_AUTH_RESPONSE — Login result
- [x] SMSG_CHAR_ENUM — Character list
- [x] SMSG_CHAR_CREATE — Character creation result
- [x] SMSG_CHAR_DELETE — Character deletion result

**World State:**
- [x] SMSG_LOGIN_VERIFY_WORLD — Initial world position
- [x] SMSG_UPDATE_OBJECT — Object creation/update (entity state)
- [x] SMSG_DESTROY_OBJECT — Object removal
- [x] SMSG_COMPRESSED_UPDATE_OBJECT — Compressed bulk updates
- [x] MSG_MOVE_* (20+ movement opcodes) — Entity movement synchronization

**Combat:**
- [x] SMSG_ATTACKSTART / SMSG_ATTACKSTOP
- [x] SMSG_ATTACKER_STATE_UPDATE — Melee hit/miss/crit/etc.
- [x] SMSG_SPELL_START / SMSG_SPELL_GO — Spell cast sequence
- [x] SMSG_SPELL_FAILURE / SMSG_SPELL_FAILED_OTHER
- [x] SMSG_AURA_UPDATE / SMSG_AURA_UPDATE_ALL — Buff/debuff tracking
- [x] SMSG_SPELL_DAMAGE_SHIELD — Reflect damage
- [x] SMSG_PERIODICAURALOG — DoT/HoT ticks
- [x] SMSG_SPELLHEALLOG / SMSG_SPELLENERGIZELOG

**Chat & Social:**
- [x] SMSG_MESSAGECHAT — Chat messages (all channels)
- [x] SMSG_CHANNEL_NOTIFY — Channel join/leave/etc.
- [x] SMSG_FRIEND_LIST / SMSG_FRIEND_STATUS
- [x] SMSG_GUILD_ROSTER / SMSG_GUILD_EVENT
- [x] SMSG_WHO — /who results
- [x] SMSG_PARTY_COMMAND_RESULT / SMSG_GROUP_LIST

**Items & Inventory:**
- [x] SMSG_INVENTORY_CHANGE_FAILURE — Error messages
- [x] SMSG_UPDATE_OBJECT with item fields
- [x] SMSG_LOOT_RESPONSE / SMSG_LOOT_RELEASE_RESPONSE
- [x] SMSG_TRADE_STATUS / SMSG_TRADE_STATUS_EXTENDED
- [x] SMSG_BUY_ITEM / SMSG_SELL_ITEM
- [x] SMSG_AUCTION_* — Auction house

**Quest:**
- [x] SMSG_QUESTGIVER_QUEST_LIST
- [x] SMSG_QUESTGIVER_QUEST_DETAILS
- [x] SMSG_QUESTGIVER_OFFER_REWARD
- [x] SMSG_QUESTGIVER_QUEST_COMPLETE
- [x] SMSG_QUEST_UPDATE_ADD_KILL / SMSG_QUEST_UPDATE_ADD_ITEM

**UI State:**
- [x] SMSG_INITIAL_SPELLS — Known spell list
- [x] SMSG_LEARNED_SPELL / SMSG_REMOVED_SPELL
- [x] SMSG_TALENT_UPDATE
- [x] SMSG_ACTION_BUTTONS — Action bar layout
- [x] SMSG_INITIALIZE_FACTIONS — Reputation
- [x] SMSG_SET_PROFICIENCY — Weapon/armor skills
- [x] SMSG_BINDPOINTUPDATE — Hearthstone location

### 4.4 Client → Server Packets (CMSG)
- [ ] Movement packets (CMSG_MOVE_*, 20+ types)
- [ ] CMSG_CAST_SPELL, CMSG_CANCEL_CAST
- [ ] CMSG_ATTACK_SWING, CMSG_ATTACK_STOP
- [ ] CMSG_GOSSIP_SELECT_OPTION, CMSG_QUEST_*
- [ ] CMSG_USE_ITEM, CMSG_SWAP_INV_ITEM, CMSG_SPLIT_ITEM
- [ ] CMSG_CHAT_MESSAGE, CMSG_JOIN_CHANNEL, CMSG_LEAVE_CHANNEL
- [ ] CMSG_WHO, CMSG_ADD_FRIEND, CMSG_DEL_FRIEND
- [ ] CMSG_GROUP_INVITE, CMSG_GROUP_ACCEPT
- [ ] CMSG_GUILD_*, CMSG_AUCTION_*, CMSG_MAIL_*

### 4.5 Entity System
- [x] Object GUIDs (64-bit with type/entry packed)
- [x] Update fields system (OBJECT_FIELD_*, UNIT_FIELD_*, PLAYER_FIELD_*, ITEM_FIELD_*, etc.)
- [x] Bitmask-based partial updates (~1400 total fields across all object types)
- [x] Object type hierarchy (Object → Item → Container, Object → Unit → Player, etc.)
- [x] Movement info struct (position, velocity, flags, transport, swimming, flying)

---

## Phase 5: Gameplay Systems

### 5.1 Player Movement
- [x] Ground movement (walk, run, strafe, backpedal)
- [x] Jump physics (parabolic arc, jump velocity)
- [x] Swimming (water detection, swim speed)
- [x] Terrain collision (walk on terrain mesh)
- [x] Heartbeat movement packets (periodic position sync)
- [ ] **TODO:** Flying (Outland/Northrend, mount speed tiers)
- [ ] **TODO:** Falling + fall damage calculation
- [ ] **TODO:** Indoor/outdoor detection
- [ ] **TODO:** Transport riding (boats, zeppelins, elevators)
- [ ] **TODO:** Movement speed modifiers (buffs, debuffs, mounts)
- [ ] **TODO:** Client-side movement prediction with server reconciliation

### 5.2 Camera System
- [x] Fly camera (development/viewer mode)
- [x] Third-person chase camera (orbiting, zoom, collision)
- [ ] **TODO:** First-person camera (zoomed all the way in)
- [ ] **TODO:** Action camera option (centered crosshair)
- [ ] **TODO:** Death/ghost camera (overhead follow)
- [ ] **TODO:** Cinematic camera (for scripted events)
- [ ] **TODO:** Vehicle camera (siege engines, etc.)

### 5.3 Targeting & Interaction
- [x] Click-to-target (raycast against character meshes)
- [x] Tab-targeting (cycle nearby enemies)
- [x] Name plates above units (health bar, name, guild, level)
- [x] Target frame UI updates
- [x] NPC interaction (gossip menus, quest dialogs, vendors)
- [x] Object interaction (mailbox, bank, forge, etc.)
- [x] Loot window
- [x] Mouseover tooltips

### 5.4 Combat (Client-Side)
The server is authoritative for all combat calculations. The client:
- [x] Sends attack/spell commands
- [x] Plays attack/cast animations based on server responses
- [x] Displays damage/healing numbers (scrolling combat text)
- [x] Shows spell effects (cast bar)
- [x] Tracks cooldowns from server data
- [x] Displays buff/debuff icons with durations
- [x] Auto-attack swing timer
- [ ] **TODO:** Range checking for abilities (client-side feedback)
- [ ] **TODO:** Line-of-sight indicators

### 5.5 Spell & Aura System (Client-Side)
- [x] Spell data loading from `Spell.dbc`
- [x] Cast bar with interrupt detection
- [x] GCD (Global Cooldown) tracking
- [x] Aura display (buffs on player, debuffs on targets)
- [x] Aura stacking and duration tracking
- [ ] **TODO:** Spell visual effects (from SpellVisual.dbc chain)

### 5.6 Inventory & Equipment
- [x] Bag system (backpack + 4 bag slots)
- [x] Item drag-and-drop
- [x] Item tooltips (stats, flavor text, set bonuses)
- [x] Equipment slots (character paper doll)
- [x] Item quality colors (grey → white → green → blue → purple → orange)
- [x] Vendor buy/sell
- [ ] **TODO:** Item comparison tooltips
- [ ] **TODO:** Bank (personal + guild bank)
- [ ] **TODO:** Item socketing (gems)
- [ ] **TODO:** Item enchanting display

### 5.7 Quest System
- [x] Quest log (25 quest limit)
- [x] Quest tracker (objectives on screen)
- [x] Quest NPC indicators (! and ? markers)
- [x] Quest dialog UI (accept/decline/complete)
- [x] Quest reward selection
- [ ] **TODO:** Minimap quest objective tracking
- [ ] **TODO:** Quest item tracking in objectives

### 5.8 Talent & Skill System
- [x] Talent tree UI (3 trees per class)
- [x] Spellbook (known spells organized by school)
- [ ] **TODO:** Talent point allocation/reset
- [ ] **TODO:** Glyph system (major/minor slots)
- [ ] **TODO:** Dual spec support
- [ ] **TODO:** Profession skill UI

### 5.9 Social Systems
- [x] Chat system (Say, Yell, Whisper, Party, Raid, Guild, channels)
- [x] Friends list with online status
- [x] Guild roster, MOTD, ranks, permissions
- [x] Group/raid frames
- [x] Emote system with animations
- [ ] **TODO:** Chat bubbles above characters
- [ ] **TODO:** Chat filters and color customization
- [ ] **TODO:** Ignore list
- [ ] **TODO:** LFG/LFD system (3.3.5 Dungeon Finder)
- [ ] **TODO:** Trade window
- [ ] **TODO:** Mail system
- [ ] **TODO:** Auction House

### 5.10 Map & Navigation
- [ ] **TODO:** World map (continent overview, zone maps)
- [ ] **TODO:** Minimap:
  - Circular minimap with player arrow
  - Rotating or locked orientation
  - NPC/quest/resource tracking icons
  - Party member indicators
  - Zoom in/out
- [ ] **TODO:** Zone name display on transitions
- [ ] **TODO:** Coordinate display
- [ ] **TODO:** Flight path map with known routes
- [ ] **TODO:** Dungeon/raid maps

---

## Phase 6: UI System (Lua + FrameXML)

This is the most complex system in the client. It must be compatible with existing WoW 3.3.5 addons.

### 6.1 Lua Virtual Machine
- [x] Lua 5.1.5 embedded
- [x] Sandboxed environment (restrict os, io, debug, etc.)
- [x] Memory limit per addon
- [x] Execution time limit (prevent infinite loops)
- [x] Error handling with stack traces to chat

### 6.2 WoW Lua API

375+ API functions implemented. Grouped by priority:

**Critical (DONE):**
- [x] Frame API: `CreateFrame`, `GetParent`, `SetPoint`, `SetSize`, `Show`, `Hide`, `SetAlpha`, `SetScale`, `GetName`, `GetFrameType`, `SetFrameStrata`, `SetFrameLevel`
- [x] Texture API: `CreateTexture`, `SetTexture`, `SetTexCoord`, `SetVertexColor`, `SetBlendMode`
- [x] FontString API: `SetText`, `GetText`, `SetFont`, `SetTextColor`, `SetJustifyH`, `SetJustifyV`
- [x] Event API: `RegisterEvent`, `UnregisterEvent`, `RegisterAllEvents`, `SetScript`
- [x] Timer API: `C_Timer.After`, frame `OnUpdate` handler
- [x] Global functions: `print`, `message`, `date`, `time`, `format`, `strsplit`, `strtrim`, `tinsert`, `tremove`, `wipe`, `sort`, `pairs`, `ipairs`, `next`, `select`, `unpack`, `type`, `tostring`, `tonumber`, `pcall`, `xpcall`
- [x] String functions: `strbyte`, `strchar`, `strfind`, `strlen`, `strlower`, `strupper`, `strsub`, `strrep`, `gsub`, `gmatch`, `match`
- [x] Math functions: `abs`, `ceil`, `floor`, `max`, `min`, `mod`, `random`, `sqrt`, `sin`, `cos`, `atan2`, `pow`, `log`, `exp`

**High Priority (DONE):**
- [x] Unit API: `UnitName`, `UnitLevel`, `UnitHealth`, `UnitHealthMax`, `UnitPower`, `UnitPowerMax`, `UnitClass`, `UnitRace`, `UnitSex`, `UnitIsPlayer`, `UnitIsDead`, `UnitIsGhost`, `UnitAffectingCombat`, `UnitBuff`, `UnitDebuff`, `UnitExists`, `UnitGUID`
- [x] Target API: `TargetUnit`, `ClearTarget`, `AssistUnit`, `FocusUnit`
- [x] Spell API: `CastSpellByName`, `CastSpellByID`, `GetSpellInfo`, `GetSpellCooldown`, `IsSpellInRange`, `IsUsableSpell`, `GetSpellTexture`, `GetSpellBookItemInfo`
- [x] Action Bar API: `GetActionInfo`, `GetActionTexture`, `GetActionCooldown`, `IsActionInRange`, `HasAction`, `UseAction`, `PickupAction`, `PlaceAction`
- [x] Item API: `GetItemInfo`, `GetItemCount`, `GetContainerItemInfo`, `GetContainerNumSlots`, `UseContainerItem`, `PickupContainerItem`, `GetItemCooldown`, `GetInventoryItemLink`
- [x] Chat API: `SendChatMessage`, `ChatFrame_AddMessage`, `GetChannelName`, `JoinChannelByName`, `LeaveChannelByName`
- [x] Quest API: `GetNumQuestLogEntries`, `GetQuestLogTitle`, `SelectQuestLogEntry`, `GetQuestLogQuestText`, `GetQuestLogRewardInfo`, `AcceptQuest`, `DeclineQuest`, `CompleteQuest`

**Medium Priority (DONE):**
- [x] Talent API: `GetNumTalentTabs`, `GetTalentTabInfo`, `GetTalentInfo`
- [x] Guild API: `GetGuildInfo`, `GetNumGuildMembers`, `GetGuildRosterInfo`, `GuildRoster`
- [x] Group API: `GetNumPartyMembers`, `GetNumRaidMembers`, `GetRaidRosterInfo`
- [ ] **TODO:** Auction API: `QueryAuctionItems`, `GetAuctionItemInfo`, `PlaceAuctionBid`, `PostAuction`, `CancelAuction`
- [ ] **TODO:** Map API: `SetMapToCurrentZone`, `GetPlayerMapPosition`, `GetMapInfo`, `GetNumMapOverlays`
- [ ] **TODO:** Social API: `GetNumFriends`, `GetFriendInfo`, `AddFriend`, `RemoveFriend`, `GetNumIgnores`, `GetIgnoreName`, `AddIgnore`, `DelIgnore`
- [ ] **TODO:** Mail API: `GetInboxNumItems`, `GetInboxHeaderInfo`, `GetInboxItem`, `TakeInboxItem`, `TakeInboxMoney`, `SendMail`, `DeleteInboxItem`
- [ ] **TODO:** Tooltip API: `GameTooltip:SetUnit`, `GameTooltip:SetItem`, `GameTooltip:SetSpell`, `GameTooltip:AddLine`, `GameTooltip:Show`, `GameTooltip:Hide`
- [ ] **TODO:** Minimap API: `Minimap:SetZoom`, `GetMinimapZoom`, `Minimap:PingLocation`

**Lower Priority (NOT YET):**
- [ ] **TODO:** Macro API: `GetNumMacros`, `GetMacroInfo`, `EditMacro`, `CreateMacro`, `RunMacroText`
- [ ] **TODO:** Equipment Set API
- [ ] **TODO:** Achievement API
- [ ] **TODO:** Calendar API
- [ ] **TODO:** LFD API
- [ ] **TODO:** Currency API
- [ ] **TODO:** Glyph API
- [ ] **TODO:** Vehicle API
- [ ] **TODO:** PvP/BG/Arena API
- [ ] **TODO:** Profession/TradeSkill API

### 6.3 Event System

400+ events implemented and firing. Critical events:

**Login & Loading:**
- `PLAYER_LOGIN`, `PLAYER_ENTERING_WORLD`, `PLAYER_LEAVING_WORLD`
- `ADDON_LOADED`, `VARIABLES_LOADED`, `PLAYER_LOGOUT`
- `LOADING_SCREEN_ENABLED`, `LOADING_SCREEN_DISABLED`
- `UPDATE_BINDINGS`

**Combat:**
- `PLAYER_REGEN_DISABLED` (entering combat), `PLAYER_REGEN_ENABLED` (leaving combat)
- `UNIT_HEALTH`, `UNIT_MANA`, `UNIT_RAGE`, `UNIT_ENERGY`, `UNIT_RUNIC_POWER`
- `UNIT_AURA`, `UNIT_TARGET`, `PLAYER_TARGET_CHANGED`
- `COMBAT_LOG_EVENT_UNFILTERED` (the unified combat log — most complex event)
- `UNIT_SPELLCAST_START`, `UNIT_SPELLCAST_STOP`, `UNIT_SPELLCAST_SUCCEEDED`, `UNIT_SPELLCAST_FAILED`, `UNIT_SPELLCAST_INTERRUPTED`
- `SPELL_UPDATE_COOLDOWN`, `ACTIONBAR_UPDATE_COOLDOWN`
- `UNIT_COMBAT` — Damage/heal floating numbers

**UI State:**
- `BAG_UPDATE`, `ITEM_LOCK_CHANGED`, `ITEM_UNLOCKED`
- `QUEST_LOG_UPDATE`, `QUEST_ACCEPTED`, `QUEST_COMPLETE`
- `CHAT_MSG_SAY`, `CHAT_MSG_YELL`, `CHAT_MSG_WHISPER`, `CHAT_MSG_PARTY`, `CHAT_MSG_GUILD`, `CHAT_MSG_CHANNEL`, `CHAT_MSG_SYSTEM`
- `FRIENDLIST_UPDATE`, `GUILD_ROSTER_UPDATE`
- `ZONE_CHANGED`, `ZONE_CHANGED_NEW_AREA`, `MINIMAP_UPDATE_ZOOM`
- `UPDATE_MOUSEOVER_UNIT`, `CURSOR_UPDATE`
- `ACTIONBAR_SLOT_CHANGED`, `ACTIONBAR_UPDATE_STATE`
- `SPELLS_CHANGED`, `LEARNED_SPELL_IN_TAB`
- `CHARACTER_POINTS_CHANGED` (talent points)

### 6.4 Frame XML System
- [x] XML parser
- [x] Full FrameXML element support (19 frame types):
  - `<Ui>`, `<Frame>`, `<Button>`, `<CheckButton>`, `<EditBox>`, `<ScrollFrame>`, `<ScrollingMessageFrame>`, `<Slider>`, `<StatusBar>`, `<GameTooltip>`, `<Minimap>`, `<Model>`, `<PlayerModel>`, `<DressUpModel>`, `<ColorSelect>`, `<SimpleHTML>`, `<MessageFrame>`, `<MovieFrame>`, `<Cooldown>`
  - `<Texture>`, `<FontString>` (layer elements)
  - `<Anchor>`, `<Size>`, `<AbsDimension>`, `<RelDimension>`
  - `<Scripts>`, `<OnLoad>`, `<OnShow>`, `<OnHide>`, `<OnClick>`, `<OnUpdate>`, `<OnEvent>`, `<OnEnter>`, `<OnLeave>`, `<OnMouseDown>`, `<OnMouseUp>`, `<OnDragStart>`, `<OnDragStop>`, `<OnValueChanged>`, `<OnTextChanged>`, `<OnEnterPressed>`, `<OnEscapePressed>`
  - Template inheritance (`inherits="..."`, `virtual="true"`)
  - `<Include file="..."/>` directives
- [x] Widget → UMG mapping (Frame→Canvas, Button→Button, EditBox→EditableText, etc.)

### 6.5 Addon System
- [x] TOC file parser
- [x] Addon discovery (scan Interface/AddOns/ directory from MPQ + user folders)
- [x] Load order resolution (dependencies, OptDeps)
- [x] File loading (Lua + XML in TOC order)
- [x] SavedVariables persistence (serialize Lua tables → file)
- [x] SavedVariablesPerCharacter support
- [ ] **TODO:** Addon enable/disable management
- [ ] **TODO:** Addon memory usage display

### 6.6 Blizzard Default UI
The default WoW UI is itself a set of addons in the MPQ data files. Loading and running:
- [x] `FrameXML/` — Core UI framework (~200 Lua/XML files)
- [x] `Interface/AddOns/Blizzard_*` — Default UI addons (~40 addons)
- 5200+ frames loading successfully; most addons compatible

---

## Phase 7: Audio

### 7.1 Music
- [x] Zone-based music from `SoundEntries.dbc`
- [x] MP3 playback from MPQ files
- [x] Smooth crossfade between zones
- [ ] **TODO:** Combat music transitions
- [ ] **TODO:** Special event music (boss encounters, cinematics)

### 7.2 Ambience
- [x] Zone-specific ambient sounds (WAV files from MPQ)
- [ ] **TODO:** Day/night ambience variations
- [ ] **TODO:** Indoor/outdoor ambience switching
- [ ] **TODO:** Weather-related sounds (rain, thunder, wind)

### 7.3 Sound Effects
- [ ] **TODO:** Spell cast/impact sounds
- [ ] **TODO:** Weapon swing/hit sounds
- [ ] **TODO:** Footstep sounds (terrain-type dependent)
- [ ] **TODO:** UI sounds (button clicks, bag open/close, quest complete jingle)
- [ ] **TODO:** Emote/voice sounds
- [ ] **TODO:** 3D positional audio for world sounds
- [ ] **TODO:** NPC greeting voice lines

---

## Phase 8: Client Features

### 8.1 Login Flow
- [x] Account credential storage
- [x] Auto-login support
- [x] Login screen (WoW-themed, server selection, credentials, connecting state)
- [x] Character select screen (3D character preview, list, create/delete/enter buttons)
- [x] Character creation screen (race/class, customization, 3D preview, name input)

### 8.2 Settings
- [ ] **TODO:** Video settings:
  - Resolution, fullscreen/windowed/borderless
  - Quality presets (Low/Medium/High/Ultra)
  - View distance (terrain LOD distance)
  - Ground clutter density
  - Shadow quality
  - Anti-aliasing mode
  - VSync
- [ ] **TODO:** Audio settings:
  - Master/Music/SFX/Ambience volume sliders
  - Enable/disable categories
- [ ] **TODO:** Keybindings:
  - Full rebinding support
  - Movement, action bars, targeting, camera
  - Modifier keys (Shift, Ctrl, Alt)
- [ ] **TODO:** Interface settings:
  - Name plates (friendly/enemy/all)
  - Floating combat text
  - Action bar options
  - Chat settings
  - Minimap options

### 8.3 Loading Screens
- [x] Loading screen display from `LoadingScreens.dbc`
- [x] Progress bar with zone/continent transitions
- [x] Zone name and level range display
- [ ] **TODO:** Gameplay tips

---

## Phase 9: Performance & Optimization

### 9.1 Rendering Performance
- [ ] **TODO:** Frustum culling for all meshes
- [ ] **TODO:** Occlusion culling (UE5 built-in)
- [ ] **TODO:** LOD system:
  - Terrain: 3 LOD levels
  - M2 models: skin file LOD levels
  - WMO: portal culling
  - Impostors for distant doodads
- [ ] **TODO:** Instanced rendering for repeated meshes (trees, grass, rocks)
- [ ] **TODO:** Texture streaming with priority (nearby textures load first)
- [ ] **TODO:** Material merging (batch draw calls per terrain chunk)
- [ ] **TODO:** Nanite evaluation for terrain/WMO meshes
- [ ] **TODO:** Virtual Shadow Maps for large worlds

### 9.2 Memory Management
- [ ] **TODO:** Memory budget: 2GB target for world data
- [ ] **TODO:** Texture pool with LRU eviction
- [ ] **TODO:** Mesh pool with distance-based unloading
- [ ] **TODO:** DBC data: load on demand, cache frequently accessed
- [ ] **TODO:** Parsed M2/WMO data: shared across instances via FWowAssetCache
- [ ] **TODO:** MPQ file handles: pool and reuse

### 9.3 Threading
- [ ] **TODO:** Main thread: rendering, input, UI
- [ ] **TODO:** Background threads:
  - MPQ file extraction
  - BLP → texture decompression
  - M2/WMO mesh building
  - ADT chunk parsing
  - Network I/O
- [ ] **TODO:** Async loading with completion callbacks on game thread
- [ ] **TODO:** No main-thread stalls during streaming

### 9.4 Network Performance
- [ ] **TODO:** Packet batching for movement updates
- [ ] **TODO:** Interest management (server-side, but client should handle gracefully)
- [ ] **TODO:** Predictive movement (client-side interpolation + extrapolation)
- [ ] **TODO:** Object update throttling for distant entities

---

## Phase 10: Polish & Completeness

### 10.1 Missing World Features
- [ ] **TODO:** Fog of war on world map (unexplored areas)
- [ ] **TODO:** Flight paths (taxi) — animated path following
- [ ] **TODO:** Boat/zeppelin transports between continents
- [ ] **TODO:** Instance portals with group requirements
- [ ] **TODO:** Graveyards and spirit healers
- [ ] **TODO:** Rested XP from inns
- [ ] **TODO:** PvP flagging and contested zones

### 10.2 Dungeon & Raid Support
- [ ] **TODO:** Instance map loading (no outdoor terrain)
- [ ] **TODO:** Dungeon maps
- [ ] **TODO:** Boss encounter UI (DBM/BigWigs compatibility through addon system)
- [ ] **TODO:** Ready check, role check
- [ ] **TODO:** Master loot / group loot / need-before-greed UI

### 10.3 PvP
- [ ] **TODO:** Battleground queue and scoreboard
- [ ] **TODO:** Arena team management
- [ ] **TODO:** Honor/arena point tracking
- [ ] **TODO:** PvP-specific UI elements

### 10.4 Professions
- [ ] **TODO:** Crafting UI (recipe list, material counts)
- [ ] **TODO:** Trade skill progress bars
- [ ] **TODO:** Gathering node tracking on minimap

### 10.5 Cinematics
- [ ] **TODO:** In-game cinematic playback (AVI files from MPQ)
- [ ] **TODO:** Scripted camera sequences

---

## Implementation Priority Order

Based on the design priorities (looks good, high performance, fully playable):

### Tier 1 — Foundation (Current → Playable Viewer)
1. Migrate ProceduralMesh → UStaticMesh/USkeletalMesh for performance
2. Terrain LOD + WDL distant terrain
3. Water rendering (MH2O)
4. Sky/atmosphere system (Light.dbc)
5. M2 skeletal animation pipeline
6. Character model rendering with equipment
7. Complete DBC typed wrappers

### Tier 2 — Playable Client
8. Complete SMSG packet handlers (UPDATE_OBJECT is the big one)
9. Player movement system with collision
10. Third-person camera
11. Lua API implementation (critical functions first)
12. Default Blizzard UI loading (FrameXML from MPQ)
13. Chat system
14. Targeting and interaction
15. Combat display (animations, damage numbers, effects)

### Tier 3 — Feature Complete
16. Inventory and equipment UI
17. Quest system
18. Talent trees and spellbook
19. Social systems (friends, guild, group)
20. Audio system
21. World map and minimap
22. Spell visual effects
23. Login/character select screens

### Tier 4 — Polish
24. Loading screens
25. Settings UI
26. WMO portal culling
27. Ground clutter
28. Weather effects
29. Addon compatibility testing
30. Performance optimization pass
31. PvP/battleground/arena support
32. Profession UI
33. Dungeon finder
34. Mail and auction house

---

## Reference Projects

| Project | Best For |
|---------|----------|
| [Noggit3](https://github.com/wowdev/noggit3) | ADT terrain, placement, and outdoor lighting |
| [pywowlib](https://github.com/wowdev/pywowlib) | Readable independent format definitions |
| [WMVx](https://github.com/Frostshake/WMVx) | WotLK M2 animation and character rendering |
| [AzerothCore](https://github.com/azerothcore/azerothcore-wotlk) | Target-server protocol, opcodes, and packet behavior |

See [external dependencies and tools](../research/external-dependencies-and-tools.md) for status and caveats.

---

## Technical Constraints

- **Protocol Version:** 12340 (3.3.5a build 12340)
- **Lua Version:** 5.1.5 (not 5.2+ — WoW 3.3.5 uses 5.1)
- **Target FPS:** 60+ on mid-range GPU (RTX 3060 / RX 6700 XT class)
- **Target Memory:** < 4GB RAM total, < 2GB for world data
- **UE5 Version:** 5.8.1 target after the fork migration
- **Initial Platform:** Linux
- **Data Source:** User-supplied original WoW 3.3.5a MPQ files; currently `enUS`
- **Server:** AzerothCore 3.3.5a compatible
