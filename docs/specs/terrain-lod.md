# Terrain LOD System

## Goal
Implement multi-level terrain LOD so distant terrain is visible without rendering full-detail chunks everywhere, enabling long view distances at high FPS.

## Context
- ADT parsing done in `Source/WowData/Public/Formats/AdtParser.h`
- Terrain rendering in `Source/WowWorld/`
- WDT parsing done in `Source/WowData/Public/Formats/WdtParser.h`
- WDL files exist in MPQ but are not yet parsed — they contain low-res height maps (17x17 per tile)
- Reference: `pywowlib/file_formats/` for WDL format

## Requirements

### WDL Parser
1. Parse WDL files from MPQ (e.g., `World/Maps/Azeroth/Azeroth.wdl`)
2. Extract MARE chunks — 17x17 height values (int16) per tile that exists in WDT
3. Extract MAHO chunks — hole bitmasks for low-res terrain

### LOD Levels
1. **LOD 0 (Near)** — Full 145-vertex chunks (9x9 outer + 8x8 inner), current implementation. Range: 0-2 tiles from camera.
2. **LOD 1 (Mid)** — Simplified mesh using outer vertices only (9x9 = 81 verts per chunk). Range: 2-5 tiles.
3. **LOD 2 (Far)** — WDL low-res terrain (17x17 per tile). Range: 5-15 tiles.

### LOD Transitions
1. Smooth transitions between LOD levels (no popping)
2. Skirt geometry or stitching at LOD boundaries to prevent gaps
3. Texture splatting on LOD 0 and LOD 1; simplified single-texture on LOD 2

### Memory
1. LOD 2 tiles should be very cheap (one mesh per tile, simple material)
2. Unload LOD 0 chunks when transitioning to LOD 1
3. Keep LOD 2 loaded for all existing tiles (low memory cost)

## Architecture

### New Files
- `WowData/Public/Formats/WdlParser.h` / `.cpp` — WDL binary parser
- `WowData/Public/Formats/WdlTypes.h` — WDL data structures
- `WowWorld/WowTerrainLod.h` / `.cpp` — LOD management, mesh simplification

### WDL Format
```
MVER — version (18)
MWMO — WMO filenames (usually empty)
MWID — WMO indices
MODF — WMO placements
MARE — 17x17 int16 heights per tile (outer 17x17 grid, no inner vertices)
MAHO — 32 bytes hole bitmask per tile (optional)
```

## Acceptance Criteria
- [ ] Builds without errors
- [ ] WDL files parse correctly for Eastern Kingdoms and Kalimdor
- [ ] Distant terrain visible at 10+ tile range
- [ ] No visible gaps between LOD levels
- [ ] Screenshot from high altitude shows terrain extending to horizon
- [ ] FPS remains 60+ with full LOD system active
- [ ] Memory usage for LOD 2 terrain < 50MB per continent

## Verification Steps
1. Build the editor target using [development setup](../setup/development.md)
2. Launch the relevant map using [development setup](../setup/development.md) — fly high above Elwynn Forest
3. Screenshot — distant terrain visible, no holes/gaps at LOD boundaries
4. Check log for tile load counts at each LOD level
