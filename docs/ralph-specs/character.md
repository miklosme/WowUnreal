# Character Rendering

## Goal
Render player characters and NPCs with race/gender models, customization (skin, face, hair), and equipment display.

## Context
- Depends on: `m2-animation.md` (skeletal mesh pipeline must work first)
- M2 parser already reads model data
- DBC generic reader exists
- Reference: `wowmodelviewer/Source/games/wow/` — character compositing logic
- Reference: `pywowlib/file_formats/` — DBC field layouts for character tables

## Requirements

### Race/Gender Models
1. Load base character models from paths like `Character/Human/Male/HumanMale.m2`
2. Map race+gender → model path via `ChrRaces.dbc` + `CreatureModelData.dbc` + `CreatureDisplayInfo.dbc`
3. 10 races × 2 genders = 20 base models for WotLK

### Character Customization
1. **CharSections.dbc** — maps (race, gender, section type, variation, color) → texture file
   - Section types: skin (base body), face, facial hair, hair, underwear
2. **Composite texture** — bake customization choices into a single character texture:
   - Start with base skin texture
   - Overlay face texture in face region
   - Overlay underwear texture
   - Apply hair texture to scalp region (for hair-under-helm)
3. **Hair model** — separate geoset loaded by hair style ID
4. **Facial hair** — geoset toggled by facial hair style

### Equipment Rendering
1. **ItemDisplayInfo.dbc** — maps item display ID → model files + textures
2. Equipment slots: head, shoulders, chest, wrists, hands, waist, legs, feet, main hand, off hand, ranged, back (cloak), tabard
3. **Armor pieces** — either:
   - Geoset swaps on the character model (chest, legs, boots → toggle geosets)
   - Texture overlays on character composite texture (shirt, tabard)
   - Attachment-point models (shoulders, helm, weapons)
4. **Weapons** — separate M2 models attached to hand attachment points
5. **Cloak** — texture applied to the cloak geoset

### Attachment Points
1. M2 models define attachment points (indexed by attachment ID)
2. Key attachments: helmet (11), right hand (0), left hand (1), right shoulder (5), left shoulder (6), back (2)
3. Equipment models get parented to attachment bone transforms

## Architecture

### New Files
- `WowCharacter/WowCharacterBuilder.h/.cpp` — Composites character from race + customization + gear
- `WowCharacter/WowCharacterTexture.h/.cpp` — Bakes composite texture from CharSections
- `WowCharacter/WowEquipmentManager.h/.cpp` — Loads + attaches equipment models
- `WowData/Formats/DbcWrappers/ChrRacesDbc.h`
- `WowData/Formats/DbcWrappers/CharSectionsDbc.h`
- `WowData/Formats/DbcWrappers/CreatureDisplayInfoDbc.h`
- `WowData/Formats/DbcWrappers/ItemDisplayInfoDbc.h`

## Acceptance Criteria
- [ ] Builds without errors
- [ ] Human Male/Female base model renders with correct proportions
- [ ] Skin color variation applied via composite texture
- [ ] At least one armor set visually renders on character
- [ ] Weapon model attaches to hand
- [ ] Screenshot shows a dressed character in idle animation
- [ ] NPC models render with their correct display info

## Verification Steps
1. `./run_test.sh build`
2. `./run_test.sh` — connect to test server, enter world with a character
3. Screenshot — player character visible with gear, NPCs visible with models
4. Or in viewer mode: manually load a character M2 with customization
