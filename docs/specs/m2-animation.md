# M2 Skeletal Animation Pipeline

## Goal
Implement bone-based skeletal animation for M2 models so characters, creatures, and animated doodads play proper animations (idle, walk, run, attack, cast, death).

## Context
- M2 parser exists in `Source/WowData/Public/Formats/M2Parser.h` — already reads vertices, indices, textures, bones
- M2 types in `Source/WowData/Public/Formats/M2Types.h`
- Currently renders M2 as static meshes only
- Reference: `wowmodelviewer/Source/games/wow/` — `WoWModel.h`, `animated.h`, `Bone.h`
- Reference: `pywowlib/file_formats/m2_format.py` — bone/animation structures

## Requirements

### Bone System
1. Parse bone hierarchy from M2 (parent indices, pivot points, flags)
2. Build `USkeleton` asset at runtime from M2 bone data
3. Each bone has: parent index (-1 for root), pivot point, flags (billboard, transformed)
4. Handle billboard bones (always face camera — used for particle attachment points)

### Animation Tracks
1. Parse animation sequences from M2 header (`nAnimations` array)
2. Each sequence: animation ID (maps to `AnimationData.dbc`), start/end time, move speed, flags (looping)
3. Bone animation tracks contain translation, rotation, scale keyframes
4. Track interpolation types: none (0), linear (1), hermite (2), bezier (3)
5. Build `UAnimSequence` per animation sequence

### Skeletal Mesh
1. Convert M2 vertices (with bone indices + weights) → `USkeletalMesh`
2. Vertices have up to 4 bone influences (indices + weights)
3. Skin files (.skin) contain LOD-specific vertex/index subsets
4. Apply same materials/textures as current static mesh pipeline

### Animation Playback
1. `USkeletalMeshComponent` with `UAnimInstance` for playback
2. Map WoW animation IDs to internal animation names:
   - 0 = Stand, 1 = Death, 4 = Walk, 5 = Run, 13 = Wound (hit), 15 = SpellCastDirected, etc.
3. Animation blending between sequences (e.g., stand → walk transition)
4. Looping animations vs one-shot

### Priority Animations
Focus on these first (most visible):
- Stand (idle)
- Walk
- Run
- Attack (1H, 2H)
- SpellCastDirected / SpellCastOmni
- Death
- ReadySpell (combat idle for casters)

## Architecture

### New/Modified Files
- `WowAssets/WowSkeletalMeshBuilder.h/.cpp` — M2 → USkeletalMesh + USkeleton
- `WowAssets/WowAnimationBuilder.h/.cpp` — M2 animation tracks → UAnimSequence
- `WowData/Formats/DbcWrappers/AnimationDataDbc.h` — AnimationData.dbc wrapper
- `WowWorld/` — Update M2 spawning to use skeletal mesh for animated models
- `WowAssets/WowAssetCache` — Cache skeletal meshes + animations

### Data Flow
```
M2Parser → FM2Model (bones + animations + skin vertices)
    ↓
WowSkeletalMeshBuilder → USkeleton + USkeletalMesh
    ↓
WowAnimationBuilder → UAnimSequence[] (one per animation)
    ↓
USkeletalMeshComponent + UAnimInstance → animated model in world
```

### Static vs Animated Decision
- M2 models with 0 bones or 0 animations → use static mesh (current path)
- M2 models with bones + animations → use skeletal mesh (new path)
- Cache decision per model path

## Acceptance Criteria
- [ ] Builds without errors
- [ ] USkeleton created from M2 bone hierarchy
- [ ] At least Stand animation plays on a creature model
- [ ] Skeletal mesh renders with correct textures
- [ ] Animated doodads (torches, flags) animate in world
- [ ] Screenshot shows an NPC/creature in idle animation pose (not T-pose)
- [ ] Static doodads (trees, rocks) still render as static mesh (no regression)

## Verification Steps
1. Build the editor target using [development setup](../setup/development.md)
2. Launch the relevant map using [development setup](../setup/development.md) — fly to area with NPCs/creatures (Northshire Abbey)
3. Screenshot — creatures should be in idle pose, not T-pose or static
4. Log should show "Created skeletal mesh for X with Y bones, Z animations"
