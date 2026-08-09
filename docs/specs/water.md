# Water Rendering

## Goal
Render water, lava, and slime surfaces from MH2O ADT chunks with proper materials and visual effects.

## Context
- ADT parser exists in `Source/WowData/Public/Formats/AdtParser.h` — does NOT yet parse MH2O
- Terrain rendering in `Source/WowWorld/`
- Materials in `Source/WowAssets/` or Content/
- `LiquidType.dbc` defines liquid types (water, ocean, magma, slime)
- Reference: `pywowlib/file_formats/adt_chunks.py` for MH2O format

## Requirements

### MH2O Parsing (WowData)
1. Parse MH2O chunk from ADT files (replaces old MCLQ format)
2. Structure per chunk (each ADT has 16x16 = 256 chunks):
   - `SMLiquidChunk` — header with instance count + offset
   - `SMLiquidInstance` — liquid type, vertex format, min/max height, vertex data offset
   - Height map (9x9 or 5x5 float grid depending on flags)
   - Depth map (optional, for transparency)
   - Existence bitmap (8x8 bits — which sub-tiles have liquid)
3. Handle multiple liquid layers per chunk (rare but possible)

### Water Material (WowRenderer)
1. **Water** (type 0-3):
   - Animated UV scrolling (two scrolling normal maps)
   - Depth-based transparency (shallow = clear, deep = opaque blue)
   - Fresnel reflections (UE5 planar reflections or SSR)
   - Specular sun/moon highlights
   - Vertex color from depth map for shore blending
2. **Magma/Lava** (type 4-7):
   - Emissive orange/red material
   - Animated UV distortion
   - No transparency
   - Glow/bloom contribution
3. **Slime** (type 8-11):
   - Opaque green material
   - Slow animated UV scroll
   - Slight emissive

### Water Mesh Generation
1. Generate flat or height-mapped mesh per chunk where liquid exists
2. Use existence bitmap to only create geometry where water is present
3. Heights from MH2O height map (not flat — rivers have flowing heights)

### Ocean Plane
1. For ocean areas (detected via WDT ocean flag or liquid type), render a large flat plane at ocean height
2. Extends to horizon, below terrain

## Architecture

### New/Modified Files
- `WowData/Formats/AdtParser` — Add MH2O chunk parsing to existing ADT parser
- `WowData/Formats/AdtTypes.h` — Add `FMH2OHeader`, `FMH2OInstance`, `FMH2OChunkData`
- `WowWorld/WowWaterRenderer.h/.cpp` — Water mesh generation + component management
- `Content/Materials/M_WowWater` — Water material (or create via C++)
- `Content/Materials/M_WowLava` — Lava material
- `Content/Materials/M_WowSlime` — Slime material
- `WowData/Formats/DbcWrappers/LiquidTypeDbc.h` — Typed wrapper for LiquidType.dbc

## Acceptance Criteria
- [ ] Builds without errors
- [ ] MH2O chunks parse without crashes for Elwynn/Westfall ADTs
- [ ] Water visible in lakes/rivers (Crystal Lake in Elwynn)
- [ ] Water has animated surface (not a static plane)
- [ ] Water transparency varies with depth
- [ ] Lava renders with emissive glow (test: Burning Steppes or Blackrock)
- [ ] Ocean plane visible at coastlines
- [ ] Screenshot shows water with reflections/transparency at a lake

## Verification Steps
1. Build the editor target using [development setup](../setup/development.md)
2. Launch the relevant map using [development setup](../setup/development.md) — fly to Crystal Lake (Elwynn Forest, ~tile 32,48)
3. Screenshot — water surface visible with transparency and animation
4. Fly to coast — ocean plane visible
