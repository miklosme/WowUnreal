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
- [ ] Complete `specs/terrain-lod.md`: add LOD 1 mid-distance meshes, MAHO hole handling, and stitched/smoothed LOD transitions — reopened March 14, 2026 verification: `WowUnrealEditor` build succeeds, runtime loads WDL with MAHO holes, and screenshot `Saved/Screenshots/terrain_lod_verify_recheck.png` captures distant terrain, but `SpawnLod1Tile()` still uses a flat fallback material instead of the terrain splat material required by `specs/terrain-lod.md`, and the current transition logic is only a lowered WDL edge skirt rather than stitched/smoothed LOD boundaries
- [ ] Finish WDL distant terrain rendering without relying on `UProceduralMeshComponent`

## Phase 5: Static Objects
- [x] M2 doodad loading and mesh creation
- [x] WMO root+group loading and per-group mesh creation
- [x] Wire placements into TerrainTile from MDDF/MODF
- [x] BLP texture loading for terrain, doodads, WMOs
- [x] HISMC instancing for repeated doodads (groups by M2 model, one HISMC per unique model per tile)
- [x] Nanite for WMO static meshes (UStaticMesh via `FMeshDescription` with Nanite enabled)
- [ ] Complete `specs/static-mesh.md`: migrate terrain, water, WDL, and legacy fallback paths off `UProceduralMeshComponent`
- [ ] Improve WMO placement fidelity beyond yaw-only rotation

## Phase 6: Networking
- [x] BigNumber (OpenSSL BIGNUM wrapper with LE/BE conversion)
- [x] SRP6 client (challenge/proof/session key/M2 verification)
- [x] ARC4-drop1024 + AuthCrypt (HMAC-SHA1 key derivation)
- [x] Auth socket (TCP, full handshake, realm list)
- [x] World socket handshake, encrypted packet framing, and character enumeration
- [x] Connection manager state machine with delegate wiring
- [ ] Implement the packet handler/entity system from `specs/networking.md` (`SMSG_LOGIN_VERIFY_WORLD`, `SMSG_UPDATE_OBJECT`, `SMSG_COMPRESSED_UPDATE_OBJECT`, `SMSG_DESTROY_OBJECT`, movement, chat, spells, action buttons)
- [ ] Add world-state data structures (`WowPacketHandler`, `WowEntityManager`, `WowEntity`, `WowUpdateFields`, handler files)
- [ ] Add gameplay CMSG flows beyond login/char enum (movement, chat, combat/spells, heartbeat)

## Phase 7: Client Features
- [x] Credential storage (multi-account JSON)
- [x] Autologin (`-autologin` flag + `UGameInstanceSubsystem`)
- [x] Screenshot manager (viewport capture)
- [x] HUD (tile coords, FPS, load status)
- [ ] Replace the viewer fly camera with gameplay movement and chase camera from `specs/movement.md`
- [ ] Add targeting, interaction, and server-synced movement state

## Phase 8: WoW UI System
- [x] Lua 5.1 VM with basic sandboxed globals
- [x] XML parsing for FrameXML files, includes, scripts, and frame definitions
- [x] SavedVariables persistence (Lua table serializer, WTF-style directory layout)
- [ ] Apply template inheritance and frame creation from parsed XML into runtime widgets
- [ ] Complete widget mapping and anchor/layout/strata behavior in `FWowFrameManager`
- [ ] Replace hardcoded Lua API stubs with real implementations (`Source/WowUI/Private/LuaApi/LuaStubs.cpp`)
- [ ] Wire event dispatch to Lua `OnEvent`/`SetScript` handlers (`FWowEventSystem::FireEvent` currently only logs)
- [ ] Add the frame methods and WoW UI API surface needed by FrameXML/addons (`CreateFrame`, `SetPoint`, `RegisterEvent`, `CreateTexture`, `CreateFontString`, unit/chat/item/spell/action APIs)
- [ ] Implement addon discovery/load-order resolution for Blizzard/default addons and user addons
- [ ] Load FrameXML/UIParent/default Blizzard UI during game startup
- [ ] Add Lua sandbox memory limits and execution timeout

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
