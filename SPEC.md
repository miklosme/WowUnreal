# WowUnreal — WoW 3.3.5a Client Specification

## Overview

A fully playable World of Warcraft 3.3.5a (Wrath of the Lich King) client built on Unreal Engine 5.7. Reads original MPQ data files, connects to AzerothCore servers, and provides a complete gameplay experience with addon support via Lua scripting.

**Design Priorities:**
1. **Visual Quality** — Leverage UE5's rendering (Lumen, Nanite where applicable, modern materials) to make the original WoW world look stunning
2. **High Performance** — 60+ FPS target on mid-range hardware, efficient streaming, minimal hitching
3. **Full Gameplay** — Complete client supporting combat, questing, dungeons, raids, PvP, auction house, mail, guilds — everything a player expects
4. **Addon Compatibility** — Lua 5.1 + FrameXML system for WoW addon support

**Target Server:** AzerothCore 3.3.5a (protocol version 12340)

---

## Architecture

### Module Structure

```
WowUnreal/
├── WowData        — Binary format parsers (ADT, WDT, WDL, M2, WMO, BLP, DBC, MPQ)
├── WowAssets      — Asset conversion pipeline (BLP→Texture, M2→SkeletalMesh, etc.)
├── WowWorld       — World streaming, terrain, object management, visibility
├── WowRenderer    — Materials, shaders, lighting, sky, water, particles, effects
├── WowCharacter   — Player/NPC models, equipment, animations, mounts
├── WowGameplay    — Combat, spells, auras, inventory, quests, loot, talents
├── WowNetwork     — Auth/world sockets, packet handlers, state sync
├── WowUI          — Lua VM, FrameXML, widget system, addon loader
├── WowAudio       — Music, ambience, sound effects, voice
├── WowClient      — Login flow, character select, settings, keybinds
└── ThirdParty     — StormLib, Lua 5.1.5, OpenSSL
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
| DBC parsing | Partial | Generic reader, needs typed wrappers |
| Terrain rendering | Done | ProceduralMesh, 4-layer texture splatting |
| World streaming | Done | Camera-based tile load/unload |
| Doodad spawning | Done | M2 static mesh rendering |
| WMO spawning | Done | Group-based rendering |
| Auth networking | Done | SRP6, ARC4-drop1024, realm list |
| World socket | Done | Encrypted packet framing |
| Lua VM | Started | Lua 5.1.5 embedded, needs API bindings |
| Frame XML | Started | Parser skeleton, needs full implementation |
| Addon loader | Started | TOC parser, needs file loading |

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
- [ ] **TODO:** Typed DBC wrappers for critical tables:
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

---

## Phase 2: Terrain & World Streaming

### 2.1 Terrain Rendering
- [x] ADT chunk mesh generation (16x16 chunks per tile, 9x9 + 8x8 vertex grids)
- [x] Height map application
- [x] Per-chunk normals
- [x] 4-layer texture splatting with 3 alpha maps
- [ ] **TODO:** Vertex colors (MCCV chunks) for terrain tinting
- [ ] **TODO:** Shadow maps (MCSH chunks)
- [ ] **TODO:** Terrain LOD system
  - Near: Full 145-vertex chunks
  - Mid: Simplified mesh (outer vertices only)
  - Far: WDL low-res terrain (17x17 per tile)
- [ ] **TODO:** Ground clutter (grass/flowers from GroundEffectTexture.dbc)

### 2.2 World Streaming
- [x] Camera-based tile loading with hysteresis
- [x] WDT tile existence checks
- [x] Distance-based object streaming (doodads + WMOs)
- [ ] **TODO:** Hybrid streaming — camera-based in viewer mode, player-position-based in game mode
- [ ] **TODO:** Async loading on background threads (no main thread hitching)
- [ ] **TODO:** Loading screen with progress bar during continent/instance transitions
- [ ] **TODO:** Memory budget enforcement (target: 2GB for world data)
- [ ] **TODO:** Instance/dungeon map support (interior-only maps with no ADT terrain)

### 2.3 Water Rendering
- [ ] **TODO:** MH2O chunk parsing (liquid heights, types, flags)
- [ ] **TODO:** Water material with:
  - Animated UV scrolling
  - Depth-based transparency/color
  - Fresnel reflections
  - Specular highlights
  - Caustics on submerged terrain
- [ ] **TODO:** Lava material (emissive, animated, no transparency)
- [ ] **TODO:** Slime material (opaque green, animated)
- [ ] **TODO:** Ocean plane for deep water areas
- [ ] **TODO:** WMO liquid support (indoor water, fountains)

### 2.4 Sky & Atmosphere
- [ ] **TODO:** Light.dbc → sky color/fog interpolation based on:
  - Time of day (24-hour cycle)
  - Player position (zone-based blending)
  - Weather state
- [ ] **TODO:** Skydome with gradient colors (top, middle, horizon bands)
- [ ] **TODO:** Sun/moon positions from DBC time-of-day data
- [ ] **TODO:** Cloud layers (scrolling textures)
- [ ] **TODO:** Distance fog matching sky horizon color
- [ ] **TODO:** UE5 atmospheric fog integration
- [ ] **TODO:** Day/night cycle with smooth transitions

---

## Phase 3: Model Rendering

### 3.1 M2 Static Models (Doodads)
- [x] Vertex/index buffer extraction
- [x] Texture assignment per render pass
- [x] Basic material creation
- [ ] **TODO:** Migrate from ProceduralMesh to UStaticMesh for performance
  - ProceduralMesh has high per-instance overhead
  - UStaticMesh enables instanced rendering for repeated doodads (trees, rocks, etc.)
  - Hierarchical Instanced Static Mesh (HISM) for mass foliage
- [ ] **TODO:** Material blending modes (opaque, alpha test, alpha blend, additive)
- [ ] **TODO:** Backface culling flags per render pass
- [ ] **TODO:** Collision generation for interactive doodads

### 3.2 M2 Animated Models (Characters, Creatures, Spells)
- [ ] **TODO:** Bone hierarchy → UE5 Skeleton asset
- [ ] **TODO:** Animation sequences → UAnimSequence
  - Stand, Walk, Run, Attack, Cast, Death, etc.
  - Animation blending and transitions
  - Interpolation: linear, hermite, bezier (from M2 track types)
- [ ] **TODO:** SkeletalMesh pipeline:
  - M2 vertices with bone weights → USkeletalMesh
  - Per-bone transforms applied via animation tracks
  - LOD support (M2 skin files contain multiple LOD levels)
- [ ] **TODO:** Attachment points (helm, shoulders, weapons, effects)
- [ ] **TODO:** Particle emitters (from M2 particle data)
  - Billboard particles
  - Ribbon trails (weapon enchants, spell effects)
- [ ] **TODO:** Texture animations (UV scrolling, transform tracks)

### 3.3 Character Rendering
- [ ] **TODO:** Race/gender model loading from `CreatureDisplayInfo.dbc`
- [ ] **TODO:** Character customization compositing:
  - Skin color, face, hair style, hair color, facial hair
  - `CharSections.dbc` → texture region lookups
  - Composite character texture (body regions baked to single texture)
- [ ] **TODO:** Equipment rendering:
  - Armor pieces as additional meshes on attachment points
  - `ItemDisplayInfo.dbc` → model file + texture lookups
  - Tabard rendering with guild emblem
  - Weapon models (main hand, off hand, ranged)
  - Enchant glow effects
- [ ] **TODO:** Mount models with rider attachment
- [ ] **TODO:** Morph/transform effects (druid forms, polymorph, etc.)

### 3.4 WMO Buildings
- [x] Group mesh rendering
- [x] Material assignment
- [ ] **TODO:** Portal-based visibility culling (MOPR/MOPT chunks)
  - Only render groups visible through portal chain from camera
  - Critical for performance in cities (Stormwind, Ironforge)
- [ ] **TODO:** Interior/exterior group flags
- [ ] **TODO:** WMO lighting (MOLT chunks — colored point lights)
- [ ] **TODO:** WMO doodad sets (furniture, decorations inside buildings)
- [ ] **TODO:** WMO liquid (indoor water features)
- [ ] **TODO:** Migrate from ProceduralMesh to UStaticMesh

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
- [ ] **TODO:** Complete opcode handler set (~400 SMSG handlers needed)
- [ ] **TODO:** Packet decompression (zlib for some large packets)
- [ ] **TODO:** Latency measurement and display

### 4.3 Core Packet Handlers

**Login & Character Select:**
- [ ] SMSG_AUTH_RESPONSE — Login result
- [ ] SMSG_CHAR_ENUM — Character list
- [ ] SMSG_CHAR_CREATE — Character creation result
- [ ] SMSG_CHAR_DELETE — Character deletion result

**World State:**
- [ ] SMSG_LOGIN_VERIFY_WORLD — Initial world position
- [ ] SMSG_UPDATE_OBJECT — Object creation/update (the big one — handles all entity state)
- [ ] SMSG_DESTROY_OBJECT — Object removal
- [ ] SMSG_COMPRESSED_UPDATE_OBJECT — Compressed bulk updates
- [ ] MSG_MOVE_* (20+ movement opcodes) — Entity movement synchronization

**Combat:**
- [ ] SMSG_ATTACKSTART / SMSG_ATTACKSTOP
- [ ] SMSG_ATTACKER_STATE_UPDATE — Melee hit/miss/crit/etc.
- [ ] SMSG_SPELL_START / SMSG_SPELL_GO — Spell cast sequence
- [ ] SMSG_SPELL_FAILURE / SMSG_SPELL_FAILED_OTHER
- [ ] SMSG_AURA_UPDATE / SMSG_AURA_UPDATE_ALL — Buff/debuff tracking
- [ ] SMSG_SPELL_DAMAGE_SHIELD — Reflect damage
- [ ] SMSG_PERIODICAURALOG — DoT/HoT ticks
- [ ] SMSG_SPELLHEALLOG / SMSG_SPELLENERGIZELOG

**Chat & Social:**
- [ ] SMSG_MESSAGECHAT — Chat messages (all channels)
- [ ] SMSG_CHANNEL_NOTIFY — Channel join/leave/etc.
- [ ] SMSG_FRIEND_LIST / SMSG_FRIEND_STATUS
- [ ] SMSG_GUILD_ROSTER / SMSG_GUILD_EVENT
- [ ] SMSG_WHO — /who results
- [ ] SMSG_PARTY_COMMAND_RESULT / SMSG_GROUP_LIST

**Items & Inventory:**
- [ ] SMSG_INVENTORY_CHANGE_FAILURE — Error messages
- [ ] SMSG_UPDATE_OBJECT with item fields
- [ ] SMSG_LOOT_RESPONSE / SMSG_LOOT_RELEASE_RESPONSE
- [ ] SMSG_TRADE_STATUS / SMSG_TRADE_STATUS_EXTENDED
- [ ] SMSG_BUY_ITEM / SMSG_SELL_ITEM
- [ ] SMSG_AUCTION_* — Auction house

**Quest:**
- [ ] SMSG_QUESTGIVER_QUEST_LIST
- [ ] SMSG_QUESTGIVER_QUEST_DETAILS
- [ ] SMSG_QUESTGIVER_OFFER_REWARD
- [ ] SMSG_QUESTGIVER_QUEST_COMPLETE
- [ ] SMSG_QUEST_UPDATE_ADD_KILL / SMSG_QUEST_UPDATE_ADD_ITEM

**UI State:**
- [ ] SMSG_INITIAL_SPELLS — Known spell list
- [ ] SMSG_LEARNED_SPELL / SMSG_REMOVED_SPELL
- [ ] SMSG_TALENT_UPDATE
- [ ] SMSG_ACTION_BUTTONS — Action bar layout
- [ ] SMSG_INITIALIZE_FACTIONS — Reputation
- [ ] SMSG_SET_PROFICIENCY — Weapon/armor skills
- [ ] SMSG_BINDPOINTUPDATE — Hearthstone location

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
- [ ] **TODO:** Object GUIDs (64-bit with type/entry packed)
- [ ] **TODO:** Update fields system:
  - OBJECT_FIELD_*, UNIT_FIELD_*, PLAYER_FIELD_*, ITEM_FIELD_*, etc.
  - Bitmask-based partial updates
  - ~1400 total fields across all object types
- [ ] **TODO:** Object type hierarchy:
  - Object → Item → Container
  - Object → Unit → Player
  - Object → GameObject
  - Object → DynamicObject
  - Object → Corpse
- [ ] **TODO:** Movement info struct (position, velocity, flags, transport, swimming, flying)

---

## Phase 5: Gameplay Systems

### 5.1 Player Movement
- [ ] **TODO:** Ground movement (walk, run, strafe, backpedal)
- [ ] **TODO:** Jump physics (parabolic arc, jump velocity)
- [ ] **TODO:** Swimming (water detection, swim speed, breath timer)
- [ ] **TODO:** Flying (Outland/Northrend, mount speed tiers)
- [ ] **TODO:** Falling + fall damage calculation
- [ ] **TODO:** Terrain collision (walk on terrain mesh, slope limits)
- [ ] **TODO:** Indoor/outdoor detection
- [ ] **TODO:** Transport riding (boats, zeppelins, elevators)
- [ ] **TODO:** Movement speed modifiers (buffs, debuffs, mounts)
- [ ] **TODO:** Client-side movement prediction with server reconciliation
- [ ] **TODO:** Heartbeat movement packets (periodic position sync)

### 5.2 Camera System
- [x] Fly camera (development/viewer mode)
- [ ] **TODO:** Third-person chase camera:
  - Orbiting around player character
  - Mouse-drag rotation (left = turn character, right = orbit camera)
  - Scroll wheel zoom (min/max distance)
  - Camera collision with terrain/buildings
  - Smooth interpolation
- [ ] **TODO:** First-person camera (zoomed all the way in)
- [ ] **TODO:** Action camera option (centered crosshair)
- [ ] **TODO:** Death/ghost camera (overhead follow)
- [ ] **TODO:** Cinematic camera (for scripted events)
- [ ] **TODO:** Vehicle camera (siege engines, etc.)

### 5.3 Targeting & Interaction
- [ ] **TODO:** Click-to-target (raycast against character meshes)
- [ ] **TODO:** Tab-targeting (cycle nearby enemies)
- [ ] **TODO:** Name plates above units (health bar, name, guild, level)
- [ ] **TODO:** Target frame UI updates
- [ ] **TODO:** NPC interaction (gossip menus, quest dialogs, vendors)
- [ ] **TODO:** Object interaction (mailbox, bank, forge, etc.)
- [ ] **TODO:** Loot window
- [ ] **TODO:** Mouseover tooltips

### 5.4 Combat (Client-Side)
The server is authoritative for all combat calculations. The client:
- [ ] **TODO:** Sends attack/spell commands
- [ ] **TODO:** Plays attack/cast animations based on server responses
- [ ] **TODO:** Displays damage/healing numbers (scrolling combat text)
- [ ] **TODO:** Shows spell effects (cast bar, projectiles, impacts)
- [ ] **TODO:** Tracks cooldowns from server data
- [ ] **TODO:** Displays buff/debuff icons with durations
- [ ] **TODO:** Auto-attack swing timer
- [ ] **TODO:** Range checking for abilities (client-side feedback)
- [ ] **TODO:** Line-of-sight indicators

### 5.5 Spell & Aura System (Client-Side)
- [ ] **TODO:** Spell data loading from `Spell.dbc`
- [ ] **TODO:** Cast bar with interrupt detection
- [ ] **TODO:** GCD (Global Cooldown) tracking
- [ ] **TODO:** Aura display (buffs on player, debuffs on targets)
- [ ] **TODO:** Aura stacking and duration tracking
- [ ] **TODO:** Spell visual effects (from SpellVisual.dbc chain)

### 5.6 Inventory & Equipment
- [ ] **TODO:** Bag system (backpack + 4 bag slots)
- [ ] **TODO:** Item drag-and-drop
- [ ] **TODO:** Item tooltips (stats, flavor text, set bonuses)
- [ ] **TODO:** Equipment slots (character paper doll)
- [ ] **TODO:** Item quality colors (grey → white → green → blue → purple → orange)
- [ ] **TODO:** Item comparison tooltips
- [ ] **TODO:** Bank (personal + guild bank)
- [ ] **TODO:** Vendor buy/sell
- [ ] **TODO:** Item socketing (gems)
- [ ] **TODO:** Item enchanting display

### 5.7 Quest System
- [ ] **TODO:** Quest log (25 quest limit)
- [ ] **TODO:** Quest tracker (objectives on screen)
- [ ] **TODO:** Quest NPC indicators (! and ? markers)
- [ ] **TODO:** Quest dialog UI (accept/decline/complete)
- [ ] **TODO:** Quest reward selection
- [ ] **TODO:** Minimap quest objective tracking
- [ ] **TODO:** Quest item tracking in objectives

### 5.8 Talent & Skill System
- [ ] **TODO:** Talent tree UI (3 trees per class)
- [ ] **TODO:** Talent point allocation/reset
- [ ] **TODO:** Glyph system (major/minor slots)
- [ ] **TODO:** Dual spec support
- [ ] **TODO:** Spellbook (known spells organized by school)
- [ ] **TODO:** Profession skill UI

### 5.9 Social Systems
- [ ] **TODO:** Chat system:
  - Say, Yell, Whisper, Party, Raid, Guild, channels
  - Chat input with /command parsing
  - Chat bubbles above characters
  - Chat filters and color customization
- [ ] **TODO:** Friends list with online status
- [ ] **TODO:** Ignore list
- [ ] **TODO:** Guild roster, MOTD, ranks, permissions
- [ ] **TODO:** Group/raid frames
- [ ] **TODO:** LFG/LFD system (3.3.5 Dungeon Finder)
- [ ] **TODO:** Emote system with animations
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
- [ ] **TODO:** Sandboxed environment (restrict os, io, debug, etc.)
- [ ] **TODO:** Memory limit per addon
- [ ] **TODO:** Execution time limit (prevent infinite loops)
- [ ] **TODO:** Error handling with stack traces to chat

### 6.2 WoW Lua API

~1200 API functions need to be implemented. Grouped by priority:

**Critical (needed for basic addon functionality):**
- [ ] Frame API: `CreateFrame`, `GetParent`, `SetPoint`, `SetSize`, `Show`, `Hide`, `SetAlpha`, `SetScale`, `GetName`, `GetFrameType`, `SetFrameStrata`, `SetFrameLevel`
- [ ] Texture API: `CreateTexture`, `SetTexture`, `SetTexCoord`, `SetVertexColor`, `SetBlendMode`
- [ ] FontString API: `SetText`, `GetText`, `SetFont`, `SetTextColor`, `SetJustifyH`, `SetJustifyV`
- [ ] Event API: `RegisterEvent`, `UnregisterEvent`, `RegisterAllEvents`, `SetScript`
- [ ] Timer API: `C_Timer.After`, frame `OnUpdate` handler
- [ ] Global functions: `print`, `message`, `date`, `time`, `format`, `strsplit`, `strtrim`, `tinsert`, `tremove`, `wipe`, `sort`, `pairs`, `ipairs`, `next`, `select`, `unpack`, `type`, `tostring`, `tonumber`, `pcall`, `xpcall`
- [ ] String functions: `strbyte`, `strchar`, `strfind`, `strlen`, `strlower`, `strupper`, `strsub`, `strrep`, `gsub`, `gmatch`, `match`
- [ ] Math functions: `abs`, `ceil`, `floor`, `max`, `min`, `mod`, `random`, `sqrt`, `sin`, `cos`, `atan2`, `pow`, `log`, `exp`

**High Priority (core gameplay UI):**
- [ ] Unit API: `UnitName`, `UnitLevel`, `UnitHealth`, `UnitHealthMax`, `UnitPower`, `UnitPowerMax`, `UnitClass`, `UnitRace`, `UnitSex`, `UnitIsPlayer`, `UnitIsDead`, `UnitIsGhost`, `UnitAffectingCombat`, `UnitBuff`, `UnitDebuff`, `UnitExists`, `UnitGUID`
- [ ] Target API: `TargetUnit`, `ClearTarget`, `AssistUnit`, `FocusUnit`
- [ ] Spell API: `CastSpellByName`, `CastSpellByID`, `GetSpellInfo`, `GetSpellCooldown`, `IsSpellInRange`, `IsUsableSpell`, `GetSpellTexture`, `GetSpellBookItemInfo`
- [ ] Action Bar API: `GetActionInfo`, `GetActionTexture`, `GetActionCooldown`, `IsActionInRange`, `HasAction`, `UseAction`, `PickupAction`, `PlaceAction`
- [ ] Item API: `GetItemInfo`, `GetItemCount`, `GetContainerItemInfo`, `GetContainerNumSlots`, `UseContainerItem`, `PickupContainerItem`, `GetItemCooldown`, `GetInventoryItemLink`
- [ ] Chat API: `SendChatMessage`, `ChatFrame_AddMessage`, `GetChannelName`, `JoinChannelByName`, `LeaveChannelByName`
- [ ] Quest API: `GetNumQuestLogEntries`, `GetQuestLogTitle`, `SelectQuestLogEntry`, `GetQuestLogQuestText`, `GetQuestLogRewardInfo`, `AcceptQuest`, `DeclineQuest`, `CompleteQuest`

**Medium Priority (full gameplay support):**
- [ ] Talent API: `GetNumTalentTabs`, `GetTalentTabInfo`, `GetTalentInfo`, `LearnTalent`, `GetActiveTalentGroup`, `SetActiveTalentGroup`
- [ ] Guild API: `GetGuildInfo`, `GetNumGuildMembers`, `GetGuildRosterInfo`, `GuildRoster`, `GuildInvite`, `GuildLeave`
- [ ] Group API: `GetNumPartyMembers`, `GetNumRaidMembers`, `GetRaidRosterInfo`, `InviteUnit`, `UninviteUnit`, `AcceptGroup`, `DeclineGroup`
- [ ] Auction API: `QueryAuctionItems`, `GetAuctionItemInfo`, `PlaceAuctionBid`, `PostAuction`, `CancelAuction`
- [ ] Map API: `SetMapToCurrentZone`, `GetPlayerMapPosition`, `GetMapInfo`, `GetNumMapOverlays`
- [ ] Social API: `GetNumFriends`, `GetFriendInfo`, `AddFriend`, `RemoveFriend`, `GetNumIgnores`, `GetIgnoreName`, `AddIgnore`, `DelIgnore`
- [ ] Mail API: `GetInboxNumItems`, `GetInboxHeaderInfo`, `GetInboxItem`, `TakeInboxItem`, `TakeInboxMoney`, `SendMail`, `DeleteInboxItem`
- [ ] Tooltip API: `GameTooltip:SetUnit`, `GameTooltip:SetItem`, `GameTooltip:SetSpell`, `GameTooltip:AddLine`, `GameTooltip:Show`, `GameTooltip:Hide`
- [ ] Minimap API: `Minimap:SetZoom`, `GetMinimapZoom`, `Minimap:PingLocation`

**Lower Priority (polish):**
- [ ] Macro API: `GetNumMacros`, `GetMacroInfo`, `EditMacro`, `CreateMacro`, `RunMacroText`
- [ ] Equipment Set API
- [ ] Achievement API
- [ ] Calendar API
- [ ] LFD API
- [ ] Currency API
- [ ] Glyph API
- [ ] Vehicle API
- [ ] PvP/BG/Arena API
- [ ] Profession/TradeSkill API

### 6.3 Event System

The client must fire ~400 events that addons can register for. Critical events:

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
- [x] XML parser skeleton
- [ ] **TODO:** Full FrameXML element support:
  - `<Ui>`, `<Frame>`, `<Button>`, `<CheckButton>`, `<EditBox>`, `<ScrollFrame>`, `<ScrollingMessageFrame>`, `<Slider>`, `<StatusBar>`, `<GameTooltip>`, `<Minimap>`, `<Model>`, `<PlayerModel>`, `<DressUpModel>`, `<ColorSelect>`, `<SimpleHTML>`, `<MessageFrame>`, `<MovieFrame>`, `<Cooldown>`
  - `<Texture>`, `<FontString>` (layer elements)
  - `<Anchor>`, `<Size>`, `<AbsDimension>`, `<RelDimension>`
  - `<Scripts>`, `<OnLoad>`, `<OnShow>`, `<OnHide>`, `<OnClick>`, `<OnUpdate>`, `<OnEvent>`, `<OnEnter>`, `<OnLeave>`, `<OnMouseDown>`, `<OnMouseUp>`, `<OnDragStart>`, `<OnDragStop>`, `<OnValueChanged>`, `<OnTextChanged>`, `<OnEnterPressed>`, `<OnEscapePressed>`
  - Template inheritance (`inherits="..."`, `virtual="true"`)
  - `<Include file="..."/>` directives
- [ ] **TODO:** Widget → UMG mapping:
  - Frame → UCanvasPanel
  - Button → UButton + overlay widgets
  - EditBox → UEditableTextBox
  - Slider → USlider
  - StatusBar → UProgressBar
  - ScrollFrame → UScrollBox
  - Texture → UImage
  - FontString → UTextBlock
  - Model → Viewport widget with 3D render target

### 6.5 Addon System
- [x] TOC file parser
- [ ] **TODO:** Addon discovery (scan Interface/AddOns/ directory from MPQ + user folders)
- [ ] **TODO:** Load order resolution (dependencies, OptDeps)
- [ ] **TODO:** File loading (Lua + XML in TOC order)
- [ ] **TODO:** SavedVariables persistence (serialize Lua tables → file)
- [ ] **TODO:** SavedVariablesPerCharacter support
- [ ] **TODO:** Addon enable/disable management
- [ ] **TODO:** Addon memory usage display
- [ ] **TODO:** Default Blizzard UI loading (the base FrameXML from MPQ)

### 6.6 Blizzard Default UI
The default WoW UI is itself a set of addons in the MPQ data files. Must load and run:
- [ ] **TODO:** `FrameXML/` — Core UI framework (~200 Lua/XML files)
- [ ] **TODO:** `Interface/AddOns/Blizzard_*` — Default UI addons (~40 addons)
- This is the ultimate integration test — if the default UI runs, most addons will work

---

## Phase 7: Audio

### 7.1 Music
- [ ] **TODO:** Zone-based music from `SoundEntries.dbc` + `ZoneMusic.dbc`
- [ ] **TODO:** MP3 playback from MPQ files
- [ ] **TODO:** Smooth crossfade between zones
- [ ] **TODO:** Combat music transitions
- [ ] **TODO:** Special event music (boss encounters, cinematics)

### 7.2 Ambience
- [ ] **TODO:** Zone-specific ambient sounds (birds, wind, water, crowds)
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
- [ ] **TODO:** Login screen:
  - Server selection (realm list)
  - Username/password input
  - "Connecting..." state with progress
  - Error messages (wrong password, banned, etc.)
  - 3D background scene (Northrend gate)
- [ ] **TODO:** Character select screen:
  - 3D character preview (animated idle)
  - Character list with name/level/race/class/zone
  - Create/delete character buttons
  - Enter World button
- [ ] **TODO:** Character creation screen:
  - Race/class selection
  - Customization options (face, hair, etc.)
  - 3D preview with rotation
  - Name input with validation

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
- [ ] **TODO:** Loading screen display from `LoadingScreens.dbc`
- [ ] **TODO:** Progress bar
- [ ] **TODO:** Zone name and level range display
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

| Project | Best For | Location |
|---------|----------|----------|
| **noggit3** | ADT/terrain structs, Lua scripting, outdoor lighting | ~/projects/noggit3 |
| **pywowlib** | Complete format definitions (most readable) | ~/projects/pywowlib |
| **wowmodelviewer** | M2 animation pipeline, character rendering | ~/projects/wowmodelviewer |
| **WMVx** | Modern model viewer reference | ~/projects/WMVx |
| **azerothcore-wotlk** | Network protocol, opcodes, packet structures | ~/projects/azerothcore-wotlk |
| **WowGodot** | Visual validation only (last resort) | ~/projects/WowGodot |

---

## Technical Constraints

- **Protocol Version:** 12340 (3.3.5a build 12340)
- **Lua Version:** 5.1.5 (not 5.2+ — WoW 3.3.5 uses 5.1)
- **Target FPS:** 60+ on mid-range GPU (RTX 3060 / RX 6700 XT class)
- **Target Memory:** < 4GB RAM total, < 2GB for world data
- **UE5 Version:** 5.7
- **Platforms:** Windows (primary), macOS (secondary)
- **Data Source:** Original WoW 3.3.5a MPQ files (not distributed with client)
- **Server:** AzerothCore 3.3.5a compatible
