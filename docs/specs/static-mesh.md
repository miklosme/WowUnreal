# Static Mesh Migration

## Goal
Migrate terrain, doodads, and WMOs from UProceduralMeshComponent to UStaticMesh/UStaticMeshComponent for massive performance gains (instancing, GPU batching, reduced per-instance overhead).

## Context
- Terrain chunks currently use `UProceduralMeshComponent` in WowWorld module
- M2 doodads spawned as individual ProceduralMesh actors
- WMO groups rendered as ProceduralMesh
- Key files: `Source/WowWorld/`, `Source/WowAssets/`
- ProceduralMesh prevents instanced rendering of repeated doodads (trees, rocks)

## Requirements

1. **Runtime UStaticMesh creation** — Build `UStaticMesh` assets at runtime from parsed vertex/index data using `FMeshDescription` or `UStaticMesh::Build()`
2. **Terrain chunks** — Each ADT chunk (16x16 per tile = 256 chunks) becomes a UStaticMesh rendered via UStaticMeshComponent
3. **M2 doodads** — Each unique M2 model creates ONE UStaticMesh, reused via:
   - `UInstancedStaticMeshComponent` for small doodads (< 100 instances)
   - `UHierarchicalInstancedStaticMeshComponent` for mass foliage/rocks (100+ instances)
4. **WMO groups** — Each WMO group mesh → UStaticMesh with proper materials
5. **Material preservation** — All existing texture/material assignments must carry over
6. **LOD support** — UStaticMesh LOD levels for distance-based quality reduction

## Architecture

### New/Modified Classes
- `WowAssets/WowStaticMeshBuilder` — Converts parsed mesh data → `UStaticMesh`
  - `UStaticMesh* BuildFromM2(const FM2Model&)`
  - `UStaticMesh* BuildFromWmoGroup(const FWmoGroup&)`
  - `UStaticMesh* BuildTerrainChunk(const FAdtChunkMesh&)`
- `WowWorld/` — Update spawning code to use `UStaticMeshComponent` / HISM
- `WowAssets/WowAssetCache` — Cache built UStaticMesh assets by path, reuse across instances

### Data Flow
```
M2Parser → FM2Model → WowStaticMeshBuilder → UStaticMesh (cached)
                                                    ↓
                              HISM/ISM Component (per-instance transforms)
```

## Acceptance Criteria
- [ ] Builds without errors
- [ ] Terrain renders identically to current ProceduralMesh version
- [ ] Doodads render with instancing (verify via `stat SceneRendering` — draw call count should drop significantly)
- [ ] WMOs render correctly
- [ ] Screenshot shows same visual quality as before migration
- [ ] Frame rate improves in dense areas (Elwynn Forest, Stormwind approach)
- [ ] No ProceduralMeshComponent usage remains for world rendering

## Verification Steps
1. Build the editor target using [development setup](../setup/development.md)
2. Launch the relevant map using [development setup](../setup/development.md) — fly to Elwynn Forest
3. Screenshot — terrain + trees + buildings visible
4. Check log for draw call count or use UE5 `stat SceneRendering`
