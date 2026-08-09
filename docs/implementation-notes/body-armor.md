# WoW Body Armor Rendering System Implementation

> Point-in-time implementation note from the original repository. Verify all paths and behavior against the current source.

This document describes the implementation of the WoW 3.3.5 body armor rendering system in this UE5 project.

## Overview

The implementation provides two core features:
1. **Equipment texture overlays** - Equipment textures are composited onto character body regions
2. **Equipment geoset visibility** - Equipment changes which parts of the character mesh are visible

## Implementation Details

### Part 1: FBodyEquipment Struct

Added `FBodyEquipment` to `FCharacterParams` in `WowCharacterBuilder.h`:

```cpp
struct FBodyEquipment
{
    uint32 ChestDisplayId = 0;
    uint32 PantsDisplayId = 0;
    uint32 BootsDisplayId = 0;
    uint32 GlovesDisplayId = 0;
    uint32 BracersDisplayId = 0;
    uint32 ShirtDisplayId = 0;
    uint32 TabardDisplayId = 0;
    uint32 BeltDisplayId = 0;
    uint32 CapeDisplayId = 0;
};
```

### Part 2: Equipment Texture Overlays

Implemented `ApplyEquipmentOverlays` in `WowCharacterTexture.cpp`:

- Loads equipment textures from `Item\TextureComponents\{overlay_path}.blp`
- Applies overlays to specific character body regions based on equipment type:

| Equipment Type | Affected Regions |
|---------------|-----------------|
| CHEST/SHIRT | ARM_UPPER, ARM_LOWER, TORSO_UPPER, TORSO_LOWER |
| ROBE (chest with leg coverage) | + LEG_UPPER, LEG_LOWER |
| PANTS | LEG_UPPER, LEG_LOWER |
| GLOVES | ARM_LOWER, HAND |
| BOOTS | LEG_LOWER, FOOT |
| BRACERS | ARM_LOWER |
| BELT | TORSO_LOWER, LEG_UPPER |
| TABARD | TORSO_UPPER, TORSO_LOWER |

### Part 3: Equipment Geoset Changes

Implemented `ComputeGeosetWithEquipment` in `WowCharacterBuilder.cpp`:

- Modifies character geoset visibility based on equipped items
- Maps equipment to geoset groups:

| Equipment | Geoset Group | Effect |
|-----------|--------------|--------|
| CHEST/SHIRT | Group 8 (wristbands) | Uses geosetGroup[0] |
| CHEST (robe) | Group 13 (trousers) | Uses geosetGroup[2] |
| PANTS | Groups 9 (kneepads), 13 (trousers) | Uses geosetGroup[1], [2] |
| GLOVES | Group 4 (gloves) | Uses geosetGroup[0] |
| BOOTS | Group 5 (boots) | Uses geosetGroup[0] |
| TABARD | Group 12 (tabard) | Always visible when equipped |
| CAPE | Group 15 (cape) | Uses geosetGroup[0] |

### Part 4: Model Viewer Integration

Updated `WowModelViewerGameMode.cpp`:

- When starter gear is enabled, populates `BodyEquipment` with items that have texture overlays
- Scans ItemDisplayInfo.dbc for items with non-empty `TextureOverlays[0-7]`
- Assigns first found items to each body equipment slot

### Part 5: Shoulder Attachment Fix

Corrected shoulder model selection in `WowEquipmentManager.cpp`:

- **LeftShoulder**: Uses `ModelNames[0]` (right-hand model) attached to left bone
- **RightShoulder**: Uses `ModelNames[1]` (left-hand model) attached to right bone
- This follows WMVx convention where shoulder models are mirrored

## Technical Integration

### Texture Compositing Flow

1. `BuildCompositeTexture` creates base character texture (skin + face + hair + underwear)
2. `ApplyEquipmentOverlays` adds equipment textures on top
3. Modified texture is applied to character mesh

### Geoset Visibility Flow

1. `ComputeDefaultGeosets` determines base visibility from customization
2. `ComputeGeosetWithEquipment` applies equipment modifications
3. `ApplyGeosetVisibility` updates mesh section visibility

### Character Spawning Flow

In `SpawnCharacterWithEquipment`:
1. Build base composite texture
2. Apply equipment overlays to texture
3. Spawn character with equipment-modified geosets
4. Attach 3D equipment (weapons, shields, etc.)

## Usage

To spawn a character with body equipment:

```cpp
FWowCharacterBuilder::FCharacterParams Params;
Params.Race = FWowCharacterBuilder::ERace::Human;
Params.Gender = FWowCharacterBuilder::EGender::Male;

// Set body equipment
Params.BodyEquipment.ChestDisplayId = 1234;
Params.BodyEquipment.PantsDisplayId = 5678;
Params.BodyEquipment.BootsDisplayId = 9012;

AActor* Character = FWowCharacterBuilder::SpawnCharacterWithEquipment(
    World, Mpq, Cache, Params, Location, Rotation);
```

## Testing

The implementation can be tested in the Model Viewer:
1. Enable "Show Starter Gear"
2. The system will automatically find and equip items with texture overlays
3. Both texture overlays and geoset changes should be visible on the character

## Files Modified

- `Source/WowAssets/Public/WowCharacterBuilder.h` - Added FBodyEquipment struct and ComputeGeosetWithEquipment
- `Source/WowAssets/Private/WowCharacterBuilder.cpp` - Implemented geoset computation and equipment integration
- `Source/WowAssets/Public/WowCharacterTexture.h` - Added ApplyEquipmentOverlays declaration
- `Source/WowAssets/Private/WowCharacterTexture.cpp` - Implemented texture overlay application
- `Source/WowAssets/Private/WowEquipmentManager.cpp` - Fixed shoulder model selection
- `Source/WowUnreal/WowModelViewerGameMode.cpp` - Added body equipment population for starter gear

This implementation provides a complete WoW-style body armor rendering system compatible with WoW 3.3.5 ItemDisplayInfo data.
