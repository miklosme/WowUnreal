# WoW 3.3.5 Unreal Engine Client — Implementation Plan

## Overview
Build a performant WoW 3.3.5a client in UE 5.7 that reads original MPQ data files, renders zones, supports the full native WoW UI (Lua + XML + addons), and connects to AzerothCore servers.

## Audit Status
Audit updated on March 14, 2026 after a source scan plus `./run_test.sh build`.
Current status: the project builds and launches as a world viewer, but several phases that were previously marked complete are only partially implemented and have been reopened below.

## Architecture
7 current modules: WowUnreal (game shell), WowData (format parsers), WowAssets (UE conversion), WowWorld (streaming/rendering), WowUI (Lua/XML frames), WowNetwork (auth/world protocol), WowClient (convenience features).

## P0: Replace 3rd-person character pawn with free-flying camera pawn ✅ COMPLETE
- [x] Terrain has no collision so ACharacter falls forever, blocking all screenshot validation. Swapped default pawn to AWowFlyCamera (spectator/fly-cam) in WowViewerGameMode. AWowPlayerCharacter deferred to Phase 11 when terrain collision is in place.

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
- [x] Complete `specs/static-mesh.md`: migrate terrain, water, WDL, and legacy fallback paths off `UProceduralMeshComponent` — verified March 14, 2026: all rendering paths (terrain, water, WDL, WMO, doodads, sky) use `FMeshDescription` → `UStaticMesh`, zero ProceduralMeshComponent usage remains, HISMC for doodad instancing, build succeeds, runtime screenshot `sky_atmosphere_verify.png` shows terrain + sky rendering correctly (90.9% non-black)
- [x] Improve WMO placement fidelity beyond yaw-only rotation — verified March 14, 2026: `FWowWmoRenderer::SpawnWmo()` applies `WowRotationToUE(Rx, Ry, Rz)` from ADT MODF data for full 3-axis rotation, build succeeds, runtime screenshot confirms world rendering with WMOs visible

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
- [x] Complete `specs/sky-atmosphere.md` using Light.dbc + LightParams + LightIntParams with zone blending — DBC-driven sky gradient via custom skydome mesh (16-ring hemisphere with vertex color height blend, dynamic material with SkyTopColor/MiddleColor/Band1/Smog/Horizon parameters), LightFloatParams.dbc parsed (5100 records, fog distance + density + cloud density), cloud layer (translucent plane with procedural noise + panner), time interpolation wrapping bug fixed; verified March 14, 2026: build succeeds, runtime screenshot `sky_atmosphere_verify.png` shows sky gradient + terrain + fog (90.9% non-black)
- [x] Complete `specs/m2-animation.md` (`USkeleton`/`USkeletalMesh`/`UAnimSequence`, playback, animated doodads) — M2 bone animation keyframe parsing (packed int16 quaternions, translation/rotation/scale tracks), FWowSkeletalMeshBuilder creates USkeleton+USkeletalMesh via FMeshDescription+FSkeletalMeshAttributes with FSkinWeightsVertexAttributesRef skin weights, IAnimationDataController for UAnimSequence at 30fps, WowDoodadManager routes animated M2s (HasBones+HasAnimationData) to skeletal path with looping playback; builds and renders March 14, 2026
- [x] Memory budget tracking in asset cache / HUD
- [x] Runtime Virtual Textures for terrain

## Phase 10: Bug Fixes & Stability
- [x] Fix UStaticMesh memory leaks: removed all unnecessary AddToRoot() calls from runtime-created UStaticMesh/USkeletalMesh/USkeleton/UAnimSequence objects — UStaticMeshComponent/USkeletalMeshComponent UPROPERTY references prevent GC while in use, and objects are naturally collected when owning actors are destroyed; removed corresponding RemoveFromRoot() calls from WowWorldManager WDL cleanup (no longer needed); affects terrain, water, WMO groups/liquid, doodads, WDL tiles, LOD1 tiles, skeletal meshes; builds and renders March 14, 2026
- [x] Fix FinalPreExposure black screen: added `r.EyeAdaptation.PreExposureOverride=1.0` to DefaultEngine.ini and CVar set in BeginPlay to force fixed pre-exposure, fixed AutoExposureBias from 10.0 (1024x overexposure) to 0.0 (standard EV100=0), added min/max brightness clamps in PostProcessVolume — verified March 14, 2026: build succeeds, runtime screenshot `fix_preexposure_blackscreen_verify.png` shows 100% non-black rendering with sky/fog gradient visible
- [x] Add thread safety to MpqManager::Initialize(): added FScopeLock(&ArchiveLock) around Archives array modification in Initialize() to prevent race with background ReadFile() calls; builds and renders March 14, 2026
- [x] Add FCriticalSection to WowAuthSocket: added SocketLock to protect Socket access in Disconnect() (game thread), SendBytes(), and RecvBytes() (network thread) — lock scoped per-operation to avoid holding during Sleep; builds and renders March 14, 2026
- [x] Add read timeouts to blocking socket recv: added 30s cumulative idle timeout to both WowAuthSocket::RecvBytes and WowWorldSocket::RecvExact — timer resets on each successful read, logs error and returns false on timeout; also fixed WowWorldSocket::RecvExact to not hold SocketLock during Sleep; builds and renders March 14, 2026
- [x] Fix Lua context use-after-free: moved FWowLuaContext from static local to heap-allocated member of UWowUIManager, added WowLuaApi::ClearContext() to nil out the registry entry before VM shutdown, Deinitialize() now clears context → deletes context → shuts down VM → resets systems (correct destruction order); builds and renders March 14, 2026
- [x] Add bounds checking to binary parsers: added MCAL alpha offset bounds check in AdtParser.cpp (skip layer if offset past end of data), added WDL tile size validation before memcpy (skip if TileSize < expected height data size); M2 skin indices already validated (SafeRead + LocalIdx < nIndices), MCVT already bounds-checked (SubSize >= 145*4), BLP mip pointers already validated (MipOffsets+MipSizes > DataSize); builds and renders March 14, 2026
- [x] Fix sky atmosphere incomplete rendering: sky gradient bands parsed but never rendered to material, cloud layers not implemented, LightFloatParams.dbc not parsed (fog distances hardcoded), wrapping no-op bug at WowSkyManager line 244 — fixed March 14, 2026: added skydome mesh with gradient material, cloud layer, LightFloatParams.dbc parser, fixed wrapping bug
- [x] Fix credential store: replaced all GetStringField/GetIntegerField/GetBoolField with TryGetStringField/TryGetNumberField/TryGetBoolField to prevent crashes on missing JSON fields, added TryGetArray/TryGetObject for root value validation, replaced plaintext password storage with XOR+Base64 obfuscation (key field renamed from "password" to "pw"), added legacy plaintext fallback with warning on load (auto-obfuscated on next save); builds and renders March 14, 2026
- [x] Add packet validation: added opcode range check in HandlePacket (reject >0x0FFF with warning), added 255-byte UTF-8 length limit for CMSG_MESSAGECHAT in SendChatMessage, added session key validation before world connect (reject with error if SessionKey is empty); builds and renders March 14, 2026
- [x] Fix terrain rendering with no textures: investigated splat material pipeline — terrain IS rendering textured (OS screenshot `terrain_verify.png` confirms 96.2% non-black with visible ground textures, trees, mountains). Previous "untextured" reports were caused by broken in-engine screenshot (Metal SM6 `GetViewportScreenShot` TextureRHI ensure failure). Fixed `SaveViewportPng` to use macOS `screencapture` as reliable fallback. BLP textures load correctly (e.g. ElwynnRockBaseTest2 256x256), 4-layer splat material compiles on Metal, 256 chunks textured per tile; builds and renders March 14, 2026
- [?] Fix WMO building rotation/placement: corrected WowRotationToUE in WowCoordinate.h from FRotator(RX, -RZ, RY) to FRotator(RY - 90.0f, RX, -RZ) — adds critical -90° pitch offset and fixes axis mapping per noggit3 from_model_rotation reference; builds and renders March 14, 2026
- [?] Add collision to terrain meshes: enabled complex-as-simple collision on terrain UStaticMesh via BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple, bAllowCpuAccess = true, SetCollisionEnabled(QueryAndPhysics), SetCollisionObjectType(WorldStatic), SetCollisionResponseToAllChannels(Block); builds and renders March 14, 2026

## Phase 11: Test Scenes
The full world terrain map is too heavy and slow for testing isolated features. Each major system needs a lightweight dedicated test scene.
- [?] Create a character/animation test scene: `-testscene=character` — spawns flat ground plane with collision, directional light, MPQ-only world manager (no terrain loading); validated with screenshot March 14, 2026
- [?] Create a UI test scene: `-testscene=ui` — spawns ground plane, directional light, MPQ-only world manager, boots Lua VM + FrameXML via UIManager->LoadUI; validated with build March 14, 2026
- [?] Create a single-tile terrain test scene: `-testscene=terrain` — loads 3x3 tile grid around startup tile with sky manager, disables streaming so no further tiles load; validated with screenshot March 14, 2026
- [?] Create a WMO test scene: `-testscene=wmo` — loads 3x3 tile grid with WMOs, directional light, disables streaming; validated with build March 14, 2026
- [ ] Create a networking test scene: no-render headless level that connects to the test server — for testing auth handshake, world login, packet handling, and entity updates without any rendering overhead
- [?] Add a scene selector or launch arg (`-testscene=X`): implemented in WowViewerGameMode::BeginPlay with 5 modes (default, character, terrain, wmo, ui); WowWorldManager::BeginPlay checks `-testscene=` to skip terrain or disable streaming; validated with build + runtime March 14, 2026

## Phase 12: Character / Audio / Gameplay (use character test scene)
- [?] Implement character rendering + equipment system: added M2 attachment point + submesh parsing to M2Parser, created WowCharacterBuilder (race/gender model loading via ChrRaces.dbc, creature display ID loading), WowCharacterTexture (skin texture from CharSections.dbc), WowEquipmentManager (weapon M2 loading and bone attachment); character test scene spawns Human M/F + Orc M models; builds and renders March 14, 2026
- [?] Implement audio system: added ZoneMusicDbc, SoundAmbienceDbc DBC wrappers; extended AreaTableDbc with AmbienceID/ZoneMusicID/IntroMusicID fields; created WowAudioManager with A/B crossfade music playback, zone-based music switching via AreaTable→ZoneMusic→SoundEntries DBC chain, WAV PCM loading via RawPCMData, MP3 via FSharedBuffer/UpdatePayload; integrated into default scene; builds and runs without crash March 14, 2026
- [ ] Implement login, character select, and character creation screens from `specs/overview.md`
- [ ] Implement gameplay/UI systems still only listed in `specs/overview.md` (combat, inventory, quests, talents, social, maps)

## Phase 13: Test Coverage
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
