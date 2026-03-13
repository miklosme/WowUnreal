# WoW 3.3.5 Unreal Engine Client — Implementation Plan

## Overview
Build a performant WoW 3.3.5a client in UE 5.7 that reads original MPQ data files, renders zones, supports the full native WoW UI (Lua + XML + addons), and connects to AzerothCore servers.

## Architecture
7 modules: WowUnreal (game shell), WowData (format parsers), WowAssets (UE conversion), WowWorld (streaming/rendering), WowUI (Lua/XML frames), WowNetwork (auth/world protocol), WowClient (convenience features).

## Phase 1: Project Cleanup & Foundation ✅ DONE
- [x] Plan created
- [x] Delete template Variant code and Content
- [x] Update .uproject, Target.cs, Build.cs files
- [x] Create WowUnreal game classes (GameInstance, GameMode, FlyCamera, PlayerController)
- [x] Create all 7 module directories with Build.cs and module registration
- [x] Integrate StormLib (ThirdParty) for MPQ reading
- [x] Implement MpqManager (archive chain, file reading)
- [x] Download Lua 5.1.5 source for WowUI module
- [x] Implement BLP parser (DXT passthrough, paletted)
- [x] Implement DBC parser (generic record/field access)
- [x] Implement coordinate conversion utilities
- [x] Create type headers for ADT, WDT, M2, WMO with stub parsers

## Phase 2: Format Parsers ✅ DONE
- [x] ADT parser (MHDR, MCIN, MCNK chunks, heights, normals, layers, alpha maps, doodad/WMO refs)
- [x] WDT parser (tile existence grid, MPHD flags)
- [x] M2 parser (vertices, indices from .skin, textures, render passes, bones)
- [x] WMO parser (root: materials, doodad sets, portals; groups: geometry, batches)
- [ ] DBC table loading (Map, AreaTable, Light, LightParams)

## Phase 3: Terrain Rendering ✅ DONE
- [x] BLP → UTexture2D factory (DXT passthrough to GPU)
- [ ] Master terrain splat material (4 layers + 3 alpha maps)
- [x] Terrain mesh builder (145-vertex chunks → ProceduralMesh)
- [x] TerrainTile actor (256 chunk meshes + materials)
- [x] World manager with WDT loading and tile streaming
- [ ] Single tile test rendering (needs UE editor verification)

## Phase 4: World Streaming
- [ ] Camera-based tile streaming (load/unload with hysteresis)
- [ ] Async tile loader (background parse → game thread create)
- [ ] WDT-driven tile existence
- [ ] Multi-tile fly-through test

## Phase 5: Static Objects
- [ ] M2 → UStaticMesh factory
- [ ] HISMC instancing for repeated doodads
- [ ] WMO group → UStaticMesh with Nanite
- [ ] WMO doodad set spawning
- [ ] Wire placements into TerrainTile from MDDF/MODF

## Phase 6: Networking
- [ ] BigNumber (OpenSSL BIGNUM wrapper)
- [ ] SRP6 client (challenge/proof/session key)
- [ ] ARC4 + AuthCrypt (packet header encryption)
- [ ] Auth socket (TCP, challenge/proof/realmlist)
- [ ] World socket (TCP, encrypted packets)
- [ ] Connection manager state machine

## Phase 7: Client Features
- [ ] Encrypted credential storage (multi-account)
- [ ] Autologin (-autologin flag)
- [ ] Screenshot manager
- [ ] HUD (tile coords, FPS, load status)

## Phase 8: WoW UI System
- [ ] Lua 5.1 VM with sandboxed globals
- [ ] XML frame parser (FrameXML, templates, Include/Script directives)
- [ ] Frame manager (19 frame types → UMG widgets)
- [ ] Anchor layout system + strata ordering
- [ ] WoW Lua API (~50 core functions)
- [ ] Event system (80+ SMSG → WoW event mappings)
- [ ] TOC parser + addon loader with dependency resolution
- [ ] SavedVariables persistence

## Phase 9: Polish
- [ ] Terrain LOD + WDL distant terrain
- [ ] Water rendering (MH2O)
- [ ] Sky/fog from DBC Light tables
- [ ] Skeletal mesh + animation pipeline
- [ ] Memory budget enforcement
- [ ] Runtime Virtual Textures for terrain

## Test Server
- Host: 127.0.0.1
- Auth port: 3724, World port: 8085
- Account: WowTestUser / WowTestPass

## Reference Projects (~/projects/)
- noggit3: ADT/WMO/M2 struct definitions
- pywowlib: Python format parsers
- wowmodelviewer: M2 rendering pipeline
- azerothcore-wotlk: Network protocol, SRP6, opcodes
