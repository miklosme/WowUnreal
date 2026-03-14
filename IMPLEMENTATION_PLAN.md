# WoW 3.3.5 Unreal Engine Client — Implementation Plan

## Overview
Build a performant WoW 3.3.5a client in UE 5.7 that reads original MPQ data files, renders zones, supports the full native WoW UI (Lua + XML + addons), and connects to AzerothCore servers.

## Audit Status
Audit updated on March 14, 2026 after a source scan plus `./run_test.sh build`.
Current status: the project builds and launches as a world viewer, but several phases that were previously marked complete are only partially implemented and have been reopened below.

## Architecture
7 current modules: WowUnreal (game shell), WowData (format parsers), WowAssets (UE conversion), WowWorld (streaming/rendering), WowUI (Lua/XML frames), WowNetwork (auth/world protocol), WowClient (convenience features).

## Phase 1: Project Cleanup & Foundation ✅ COMPLETE
- [x] Plan created
- [x] Delete template Variant code and Content
- [x] Update `.uproject`, `Target.cs`, `Build.cs` files
- [x] Create WowUnreal game classes (GameInstance, GameMode, FlyCamera, PlayerController)
- [x] Create all 7 module directories with Build.cs and module registration
- [x] Integrate StormLib (ThirdParty) for MPQ reading
- [x] Implement MpqManager (archive chain, file reading)
- [x] Download Lua 5.1.5 source for WowUI module
- [x] Implement BLP parser (DXT passthrough, paletted)
- [x] Implement DBC parser (generic record/field access)
- [x] Implement coordinate conversion utilities
- [x] Create type headers for ADT, WDT, M2, WMO with stub parsers

## Phase 2: Format Parsers
- [x] ADT parser (MHDR, MCIN, MCNK chunks, heights, normals, layers, alpha maps, doodad/WMO refs)
- [x] WDT parser (tile existence grid, MPHD flags)
- [x] M2 parser (vertices, indices from `.skin`, textures, render passes, bones)
- [x] WMO parser (root: materials, doodad sets, portals; groups: geometry, batches)
- [x] Complete Tier 1 typed DBC wrappers from `specs/dbc-wrappers.md` (verified: builds, runtime loads 12/12 DBC tables with correct data)
- [x] Add Tier 2 typed DBC wrappers from `specs/dbc-wrappers.md` (Spell, SpellVisual, SpellVisualKit, SoundEntries, LoadingScreens, GroundEffectTexture, EmotesText, Talent, TalentTab) — verified March 14, 2026: build succeeds, runtime loads 21/21 DBC tables, screenshot `Saved/Screenshots/dbc_tier2_wrappers_verify.png`

## Phase 3: Terrain Rendering
- [x] BLP → UTexture2D factory (DXT passthrough to GPU)
- [x] Master terrain splat material
- [x] Terrain mesh builder (145-vertex chunks → ProceduralMesh)
- [x] TerrainTile actor (256 chunk meshes + materials)
- [x] World manager with WDT loading and tile streaming
- [x] Single tile test rendering (build/run audit still loads and renders terrain tiles)
- [x] Fix terrain material compile/fallback warnings on Metal (suppressed spurious LoadObject warning, extracted shared ChunkId.h to fix unity build collisions, runtime alpha samplers now compile on SF_METAL_SM6) — verified March 14, 2026: `WowUnrealEditor` build succeeds, runtime logs `LogTerrainMat` material creation/compile on Metal without fallback warnings, screenshot `Saved/Screenshots/terrain_material_metal_verify_recheck.png`

## Phase 4: World Streaming
- [x] Camera-based tile streaming (load/unload with hysteresis)
- [x] WDT-driven tile existence
- [x] Async tile loader (background thread with `TFuture`, game-thread finalization)
- [x] Multi-tile viewer streaming works during runtime audit
- [x] Complete `specs/terrain-lod.md`: add LOD 1 mid-distance meshes, MAHO hole handling, and stitched/smoothed LOD transitions — verified March 14, 2026: build succeeds, code review confirms LOD 1 (81-vert chunks with per-chunk splat materials), MAHO hole bitmask parsing/skipping, and ADT-driven transition strips; runtime screenshot shows textured distant terrain extending to horizon
- [x] Finish WDL distant terrain rendering without relying on `UProceduralMeshComponent` — verified March 14, 2026: code review confirms `SpawnWdlTile` builds `FMeshDescription` into `UStaticMesh`/`UStaticMeshComponent` with zero `ProceduralMeshComponent` in WDL code; `WowUnrealEditor` builds (0 errors), runtime screenshot `wdl_staticmesh_verify_current2.png` shows distant terrain geometry rendering correctly

## Phase 5: Static Objects
- [x] M2 doodad loading and mesh creation
- [x] WMO root+group loading and per-group mesh creation
- [x] Wire placements into TerrainTile from MDDF/MODF
- [x] BLP texture loading for terrain, doodads, WMOs
- [x] HISMC instancing for repeated doodads (groups by M2 model, one HISMC per unique model per tile)
- [x] Nanite for WMO static meshes (UStaticMesh via `FMeshDescription` with Nanite enabled)
- [ ] Complete `specs/static-mesh.md`: migrate terrain, water, WDL, and legacy fallback paths off `UProceduralMeshComponent` — March 14, 2026 verification reopened: code review still shows terrain, WDL, doodads, WMOs, and current water paths using `FMeshDescription` → `UStaticMesh`, and `WowUnrealEditor` builds successfully. Runtime still fails visual verification: both the in-engine and OS-level screenshots remain black, and the launch reproduces `FinalPreExposure > 0.0f` in `PostProcessEyeAdaptation.cpp` before a usable world image is captured (`Saved/Screenshots/staticmesh_migration_verify_recheck_early_20260314.png`)
- [ ] Improve WMO placement fidelity beyond yaw-only rotation — March 14, 2026 verification reopened: code review confirms `FWowWmoRenderer::SpawnWmo()` now applies `WowRotationToUE(Rx, Ry, Rz)` from ADT `MODF` data instead of yaw-only rotation, and `WowUnrealEditor` builds successfully. Runtime still reproduces the affected Goldshire/Elwynn WMO spawns (farms, keep walls, blacksmith, inn), but the render path hits `FinalPreExposure > 0.0f` before a usable screenshot can be captured, so visual verification is still incomplete

## Phase 6: Networking
- [x] BigNumber (OpenSSL BIGNUM wrapper with LE/BE conversion)
- [x] SRP6 client (challenge/proof/session key/M2 verification)
- [x] ARC4-drop1024 + AuthCrypt (HMAC-SHA1 key derivation)
- [x] Auth socket (TCP, full handshake, realm list)
- [x] World socket handshake, encrypted packet framing, and character enumeration
- [x] Connection manager state machine with delegate wiring
- [ ] Implement the packet handler/entity system from `specs/networking.md` (`SMSG_LOGIN_VERIFY_WORLD`, `SMSG_UPDATE_OBJECT`, `SMSG_COMPRESSED_UPDATE_OBJECT`, `SMSG_DESTROY_OBJECT`, movement, chat, spells, action buttons) — March 14, 2026 verification reopened: `WowUnrealEditor` builds and live logs show `AUTH_OK`, `LOGIN_VERIFY_WORLD`, spell/action-button packets, and `UPDATE_OBJECT` / `COMPRESSED_UPDATE_OBJECT` entity traffic, but mandatory visual verification still fails because the available screenshots (`Saved/Screenshots/networking_verify_live_test.png`, `Saved/Screenshots/networking_packet_entity_live_verify_clean.png`, and OS captures) are black, and screenshot runs continue to hit `FinalPreExposure > 0.0f` before a usable world image is captured
- [ ] Add world-state data structures (`WowPacketHandler`, `WowEntityManager`, `WowEntity`, `WowUpdateFields`, handler files) — March 14, 2026 verification reopened: `WowPacketHandler`, `WowEntityManager`, `WowEntity`, and `WowUpdateFields` exist, `WowUnrealEditor` builds, and a live autologin run logs `LOGIN_VERIFY_WORLD`, `INITIAL_SPELLS`, `ACTION_BUTTONS`, `COMPRESSED_UPDATE_OBJECT`, and ongoing `UPDATE_OBJECT` entity create/destroy traffic. The task is still incomplete because the spec's `WowNetwork/Handlers/` split is absent (handlers remain monolithic in `Source/WowNetwork/Private/WowPacketHandler.cpp`), the delayed in-engine screenshot never saved, and both runtime verification captures remain unusable after the render-path `FinalPreExposure > 0.0f` ensure; OS capture `Saved/Screenshots/networking_world_state_verify_os_20260315.png` is 100% black.
- [ ] Add gameplay CMSG flows beyond login/char enum (movement, chat, combat/spells, heartbeat) — March 14, 2026 verification reopened: `WowUnrealEditor` builds and a live `-autologin` run reaches auth, character enum, `LOGIN_VERIFY_WORLD`, `INITIAL_SPELLS`, `ACTION_BUTTONS`, and ongoing `UPDATE_OBJECT` traffic. The task is still incomplete because the spec-required combat/spell client flows are missing: `Source/WowNetwork/Private/WowConnectionManager.cpp` only sends movement/chat/selection/keep-alive packets, there is no `CMSG_CAST_SPELL` or `CMSG_ATTACKSWING` send path, and `Source/WowUI/Private/LuaApi/LuaStubs.cpp` still leaves `CastSpellByName` / `CastSpellByID` stubbed. Mandatory visual verification also still fails: the render path reproduces `FinalPreExposure > 0.0f`, the engine auto-screenshot path was incorrectly expanded to `Saved/Screenshots/Saved/Screenshots/...`, no in-engine screenshot was saved, and OS capture `Saved/Screenshots/networking_cmsg_flows_verify_os_20260315.png` is 100% black.

## Phase 7: Client Features
- [x] Credential storage (multi-account JSON)
- [x] Autologin (`-autologin` flag + `UGameInstanceSubsystem`)
- [x] Screenshot manager (viewport capture)
- [x] HUD (tile coords, FPS, load status)
- [?] Replace the viewer fly camera with gameplay movement and chase camera from `specs/movement.md` — AWowPlayerCharacter (ACharacter + spring arm + chase camera), AWowGameplayController (movement sync, keep-alive), WASD movement, jump, camera orbit/zoom; builds and runs
- [?] Add targeting, interaction, and server-synced movement state — CMSG_SET_SELECTION, server spawn position teleport via LOGIN_VERIFY_WORLD, entity update listener for local player, movement sync gated by bHasServerPosition; builds

## Phase 8: WoW UI System
- [x] Lua 5.1 VM with basic sandboxed globals
- [x] XML parsing for FrameXML files, includes, scripts, and frame definitions
- [x] SavedVariables persistence (Lua table serializer, WTF-style directory layout)
- [?] Apply template inheritance and frame creation from parsed XML into runtime widgets — ResolveInherits() parses comma-separated template names, MergeTemplate() merges attributes/layers/scripts/children with override semantics, ApplyAnchors() maps WoW 9-point anchors to UMG canvas slots; builds
- [?] Complete widget mapping and anchor/layout/strata behavior in `FWowFrameManager` — strata z-ordering (1000 per level + frameLevel), 9-point anchor system, two-anchor stretch, setAllPoints fill, widget type mapping (Button, EditBox, StatusBar, Slider, Frame→Canvas); builds
- [?] Replace hardcoded Lua API stubs with real implementations (`Source/WowUI/Private/LuaApi/LuaStubs.cpp`) — unit API (UnitHealth/Level/Power/IsDead/IsPlayer/GUID/Exists) now reads from entity manager, SendChatMessage wires to ConnectionManager, ClearTarget sends CMSG_SET_SELECTION, IsLoggedIn checks session state, GetFramerate reads real FPS; FWowLuaContext stored in Lua registry; builds
- [?] Wire event dispatch to Lua `OnEvent`/`SetScript` handlers — `FWowEventSystem::FireEvent` now looks up compiled OnEvent functions via `luaL_ref` and calls them with `(self, event, ...)` args; `SetFrameScript` compiles XML inline code into `function(self, event, ...) <code> end`; `CreateFrameObject` creates Lua tables for frames (registered as globals for named frames); `FWowFrameManager::CreateFrame` calls `CompileFrameScripts` automatically; builds
- [?] Add the frame methods and WoW UI API surface needed by FrameXML/addons — `CreateFrame` global function, frame metatable with ~80 methods (RegisterEvent, UnregisterEvent, SetScript, GetScript, Show, Hide, SetPoint, SetSize, Get/SetWidth/Height, SetAlpha, SetFrameStrata/Level, CreateTexture, CreateFontString, SetBackdrop, SetID, EnableMouse/Keyboard, Button/StatusBar/EditBox/ScrollFrame/Cooldown methods), Texture and FontString stub objects with full method sets; `FWowLuaContext` extended with EventSystem and FrameManager pointers; builds
- [?] Implement addon discovery/load-order resolution — `DiscoverAddons` checks filesystem + 23 well-known Blizzard addon names in MPQ, `ResolveLoadOrder` does topological sort via Kahn's algorithm respecting RequiredDeps and OptionalDeps, `LoadAllAddons` orchestrates discover→sort→load, `LoadAddon` now wires Frame XML directives to `FWowFrameManager::CreateFrame`; builds
- [?] Load FrameXML/UIParent/default Blizzard UI during game startup — `UWowUIManager` GameInstanceSubsystem creates and wires LuaVM + FrameManager + EventSystem on init, `LoadUI(FMpqManager*)` loads FrameXML.toc directives (scripts + frames) then calls `LoadAllAddons` for Blizzard/user addons, fires ADDON_LOADED + PLAYER_LOGIN init events; GameMode calls `LoadUI` after WorldManager spawns; builds
- [?] Add Lua sandbox memory limits and execution timeout — custom `lua_newstate` allocator with configurable memory limit (default 128 MB, denies alloc when exceeded), instruction count hook fires every 1000 instructions with configurable limit (default 10M, raises luaL_error), counter resets per `ExecuteString`/`ExecuteBuffer` call; builds

## Phase 9: World Polish
- [ ] Complete `specs/water.md` (animated liquid materials, depth/transparency, liquid-type handling, ocean plane, WMO liquid)
- [ ] Complete `specs/sky-atmosphere.md` using Light.dbc + LightParams + LightInt/FloatParams with zone blending
- [ ] Complete `specs/m2-animation.md` (`USkeleton`/`USkeletalMesh`/`UAnimSequence`, playback, animated doodads)
- [x] Memory budget tracking in asset cache / HUD
- [x] Runtime Virtual Textures for terrain

## Phase 10: Character / Audio / Gameplay
- [ ] Implement character rendering + equipment system from `specs/character.md`
- [ ] Implement audio system from `specs/audio.md`
- [ ] Implement login, character select, and character creation screens from `specs/overview.md`
- [ ] Implement gameplay/UI systems still only listed in `specs/overview.md` (combat, inventory, quests, talents, social, maps)

## Phase 11: Test Coverage
- [ ] Add first-party automated tests for parsers, world streaming, networking, UI, and addon loading (the repo currently only contains vendored StormLib tests)

## Test Server
- Host: 127.0.0.1
- Auth port: 3724, World port: 8085
- Account: WowTestUser / WowTestPass

## Reference Projects (~/projects/)
- noggit3: ADT/WMO/M2 struct definitions
- pywowlib: Python format parsers
- wowmodelviewer: M2 rendering pipeline
- azerothcore-wotlk: Network protocol, SRP6, opcodes
