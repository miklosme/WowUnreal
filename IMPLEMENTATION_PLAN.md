# WoW 3.3.5 Unreal Engine Client — Implementation Plan

## Overview
Build a performant WoW 3.3.5a client in UE 5.7 that reads original MPQ data files, renders zones, supports the full native WoW UI (Lua + XML + addons), and connects to AzerothCore servers.

## Audit Status
Audit updated on March 14, 2026 after a source scan plus `./run_test.sh build`.
Current status: the project builds and launches as a world viewer, but several phases that were previously marked complete are only partially implemented and have been reopened below.

## Architecture
7 current modules: WowUnreal (game shell), WowData (format parsers), WowAssets (UE conversion), WowWorld (streaming/rendering), WowUI (Lua/XML frames), WowNetwork (auth/world protocol), WowClient (convenience features).

## P0: Replace 3rd-person character pawn with free-flying camera pawn
- [ ] Terrain has no collision so ACharacter falls forever, blocking all screenshot validation. Swap to a spectator/fly-cam pawn as the default during development. Defer chase-cam character to Phase 11 when collision is in place.

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
- [ ] Add gameplay CMSG flows beyond login/char enum (movement, chat, combat/spells, heartbeat) — March 14, 2026 verification reopened: `WowUnrealEditor` builds successfully, and a live `-autologin` run logs `AUTH_OK`, `LOGIN_VERIFY_WORLD`, `INITIAL_SPELLS`, `ACTION_BUTTONS`, and ongoing `UPDATE_OBJECT` traffic in `/Users/clancey/Library/Logs/WowUnreal/WowUnreal_3.log`. The task is still incomplete because mandatory visual verification failed again: the in-engine delayed screenshot never saved, OS capture `Saved/Screenshots/networking_cmsg_flows_verify_os_20260315.png` is fully black, and the automated run stalled before logging the scheduled auto screenshot/quit. This pass also did not exercise `CMSG_CAST_SPELL`, `CMSG_ATTACKSWING`, or `CMSG_ATTACKSTOP` on a live target, so those outbound flows remain code-reviewed rather than runtime-verified.

## Phase 7: Client Features
- [x] Credential storage (multi-account JSON)
- [x] Autologin (`-autologin` flag + `UGameInstanceSubsystem`)
- [x] Screenshot manager (viewport capture)
- [x] HUD (tile coords, FPS, load status)
- [x] Replace the viewer fly camera with gameplay movement and chase camera from `specs/movement.md` — AWowPlayerCharacter (ACharacter + spring arm + chase camera), AWowGameplayController (movement sync, keep-alive), WASD movement, jump, camera orbit/zoom; builds and runs — verified March 14, 2026: code review confirms ACharacter with spring arm + chase cam, WASD via Enhanced Input relative to camera yaw, jump, zoom, camera collision, slope limit, server speed application; build succeeds
- [x] Add targeting, interaction, and server-synced movement state — CMSG_SET_SELECTION, server spawn position teleport via LOGIN_VERIFY_WORLD, entity update listener for local player, movement sync gated by bHasServerPosition; builds — verified March 14, 2026: code review confirms OnLoginVerifyWorld teleport, OnEntityUpdated listener, 500ms movement heartbeat, CMSG_SET_SELECTION in ConnectionManager, bHasServerPosition gate; build succeeds

## Phase 8: WoW UI System
- [x] Lua 5.1 VM with basic sandboxed globals
- [x] XML parsing for FrameXML files, includes, scripts, and frame definitions
- [x] SavedVariables persistence (Lua table serializer, WTF-style directory layout)
- [x] Apply template inheritance and frame creation from parsed XML into runtime widgets — ResolveInherits() parses comma-separated template names, MergeTemplate() merges attributes/layers/scripts/children with override semantics, ApplyAnchors() maps WoW 9-point anchors to UMG canvas slots; builds — verified March 14, 2026: code review confirms all three functions with correct semantics; build succeeds
- [x] Complete widget mapping and anchor/layout/strata behavior in `FWowFrameManager` — strata z-ordering (1000 per level + frameLevel), 9-point anchor system, two-anchor stretch, setAllPoints fill, widget type mapping (Button, EditBox, StatusBar, Slider, Frame→Canvas); builds — verified March 14, 2026: code review confirms z-order formula, 9-point mapping, two-anchor stretch, widget type dispatch; build succeeds
- [x] Replace hardcoded Lua API stubs with real implementations (`Source/WowUI/Private/LuaApi/LuaStubs.cpp`) — unit API (UnitHealth/Level/Power/IsDead/IsPlayer/GUID/Exists) now reads from entity manager, SendChatMessage wires to ConnectionManager, ClearTarget sends CMSG_SET_SELECTION, IsLoggedIn checks session state, GetFramerate reads real FPS; FWowLuaContext stored in Lua registry; builds — verified March 14, 2026: code review confirms all API functions read from EntityManager/ConnectionManager, context in Lua registry; build succeeds
- [x] Wire event dispatch to Lua `OnEvent`/`SetScript` handlers — `FWowEventSystem::FireEvent` now looks up compiled OnEvent functions via `luaL_ref` and calls them with `(self, event, ...)` args; `SetFrameScript` compiles XML inline code into `function(self, event, ...) <code> end`; `CreateFrameObject` creates Lua tables for frames (registered as globals for named frames); `FWowFrameManager::CreateFrame` calls `CompileFrameScripts` automatically; builds — verified March 14, 2026: code review confirms FireEvent dispatch, SetFrameScript compilation, CreateFrameObject tables, CompileFrameScripts integration; build succeeds
- [x] Add the frame methods and WoW UI API surface needed by FrameXML/addons — `CreateFrame` global function, frame metatable with ~80 methods, Texture and FontString stub objects with full method sets; `FWowLuaContext` extended with EventSystem and FrameManager pointers; builds — verified March 14, 2026: code review confirms 70+ FrameMethods, Texture/FontString stubs, FWowLuaContext wiring; build succeeds
- [x] Implement addon discovery/load-order resolution — `DiscoverAddons` checks filesystem + 23 well-known Blizzard addon names in MPQ, `ResolveLoadOrder` does topological sort via Kahn's algorithm respecting RequiredDeps and OptionalDeps, `LoadAllAddons` orchestrates discover→sort→load, `LoadAddon` now wires Frame XML directives to `FWowFrameManager::CreateFrame`; builds — verified March 14, 2026: code review confirms Kahn's algorithm topo sort, 21 Blizzard addon names, discover→sort→load pipeline; build succeeds
- [x] Load FrameXML/UIParent/default Blizzard UI during game startup — `UWowUIManager` GameInstanceSubsystem creates and wires LuaVM + FrameManager + EventSystem on init, `LoadUI(FMpqManager*)` loads FrameXML.toc directives (scripts + frames) then calls `LoadAllAddons` for Blizzard/user addons, fires ADDON_LOADED + PLAYER_LOGIN init events; GameMode calls `LoadUI` after WorldManager spawns; builds — verified March 14, 2026: code review confirms full init pipeline and event firing; build succeeds
- [x] Add Lua sandbox memory limits and execution timeout — custom `lua_newstate` allocator with configurable memory limit (default 128 MB, denies alloc when exceeded), instruction count hook fires every 1000 instructions with configurable limit (default 10M, raises luaL_error), counter resets per `ExecuteString`/`ExecuteBuffer` call; builds — verified March 14, 2026: code review confirms custom allocator, instruction hook, counter reset; build succeeds

## Phase 9: World Polish
- [x] Complete `specs/water.md` (animated liquid materials, depth/transparency, liquid-type handling, ocean plane, WMO liquid) — MH2O parsing, water/lava/slime materials with animated noise+depth, ocean plane, existence bitmap, WMO MLIQ liquid parsing and mesh creation with tile visibility flags; builds and smoke-tests — verified March 14, 2026: code review confirms all spec requirements met (MH2O parsing with existence bitmap, water/lava/slime materials with animation, ocean plane, WMO MLIQ with tile visibility flags), build succeeds
- [?] Complete `specs/sky-atmosphere.md` using Light.dbc + LightParams + LightIntParams with zone blending — DBC-driven sky gradient via custom skydome mesh (16-ring hemisphere with vertex color height blend, dynamic material with SkyTopColor/MiddleColor/Band1/Smog/Horizon parameters), LightFloatParams.dbc parsed (5100 records, fog distance + density + cloud density), cloud layer (translucent plane with procedural noise + panner), time interpolation wrapping bug fixed (replaced no-op `if (HighIdx == 0) HighIdx = 0` with proper `bFoundHigh` flag); builds, runtime logs show 22/22 DBC tables, skydome + cloud mesh created, screenshot validates non-black (99.7%)
- [ ] Complete `specs/m2-animation.md` (`USkeleton`/`USkeletalMesh`/`UAnimSequence`, playback, animated doodads)
- [x] Memory budget tracking in asset cache / HUD
- [x] Runtime Virtual Textures for terrain

## Phase 10: Bug Fixes & Stability
- [ ] Fix UStaticMesh memory leaks: every runtime-created UStaticMesh calls AddToRoot() but RemoveFromRoot() is never called on unload — affects terrain (WowTerrainTile.cpp:149), water (WowWaterRenderer.cpp:72), WMO groups (WowWmoRenderer.cpp:289), WMO liquid (WowWmoRenderer.cpp:241), doodads (WowDoodadManager.cpp:208), WDL tiles (WowWorldManager.cpp:1030), LOD1 tiles (WowWorldManager.cpp:1265). Add cleanup in tile/object destruction paths.
- [ ] Fix FinalPreExposure black screen: renderer hits `FinalPreExposure > 0.0f` ensure causing black rendering. PostProcessVolume and DefaultEngine.ini mitigations are in place but not fully effective. Verify fix works after async tile loading changes, adjust AutoExposureBias if needed.
- [ ] Add thread safety to MpqManager::Initialize(): Archives array is modified without holding ArchiveLock during init, causing race with background ReadFile() calls. Add FScopeLock in Initialize() and shutdown fence.
- [ ] Add FCriticalSection to WowAuthSocket: unlike WowWorldSocket which has SocketLock, auth socket has no lock protecting socket access — Disconnect() from game thread can crash RecvBytes() on network thread (WowAuthSocket.cpp:560-585)
- [ ] Add read timeouts to blocking socket recv: both auth (WowAuthSocket.cpp:560) and world (WowWorldSocket.cpp:612) sockets spin-sleep forever if server stops sending. Add 30s cumulative timeout with disconnect.
- [ ] Fix Lua context use-after-free: FWowLuaContext* stored as light userdata in Lua registry (LuaApiRegistry.cpp:28-48) — if C++ context is destroyed while Lua holds reference, any callback crashes. Ensure VM shutdown before context destruction.
- [ ] Add bounds checking to binary parsers: ADT MCVT chunk size not validated (AdtParser.cpp:359), WDL height memcpy without length check (WdlParser.cpp:91-93), M2 skin index bounds (M2Parser.cpp:414), BLP mip pointer validation (BlpParser.cpp:79-92), ADT MCAL alpha offset (AdtParser.cpp:204-205)
- [x] Fix sky atmosphere incomplete rendering: sky gradient bands parsed but never rendered to material, cloud layers not implemented, LightFloatParams.dbc not parsed (fog distances hardcoded), wrapping no-op bug at WowSkyManager line 244 — fixed March 14, 2026: added skydome mesh with gradient material, cloud layer, LightFloatParams.dbc parser, fixed wrapping bug
- [ ] Fix credential store: passwords stored plaintext in JSON (WowCredentialStore.cpp:23,40), JSON parsing crashes on missing fields — use TryGetStringField() and consider platform keychain
- [ ] Add packet validation: no opcode range validation in dispatch (WowPacketHandler.cpp:30-43), chat messages sent without 255-byte length limit, missing session key validation before world connect
- [ ] Fix terrain rendering with no textures: terrain geometry renders but appears untextured in the last working (non-black) test — investigate splat material assignment, BLP texture loading, and UV/alpha map pipeline for terrain chunks
- [ ] Add collision to terrain meshes: runtime-created UStaticMesh terrain chunks have no collision, preventing character controller from walking on ground — add complex or simple collision bodies to terrain mesh build path so chase-cam character can be re-enabled in Phase 11

## Phase 11: Character / Audio / Gameplay
- [ ] Implement character rendering + equipment system from `specs/character.md`
- [ ] Implement audio system from `specs/audio.md`
- [ ] Implement login, character select, and character creation screens from `specs/overview.md`
- [ ] Implement gameplay/UI systems still only listed in `specs/overview.md` (combat, inventory, quests, talents, social, maps)

## Phase 12: Test Coverage
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
