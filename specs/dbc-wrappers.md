# Typed DBC Wrappers

## Goal
Create typed C++ wrappers around critical DBC tables so other systems can query game data with named fields instead of raw byte offsets.

## Context
- Generic DBC reader exists: `Source/WowData/Public/Formats/DbcParser.h`
- Generic reader returns raw records — callers must know column indices
- ~200 DBC files in MPQ, but only ~25 are critical for initial implementation
- Reference: `pywowlib/file_formats/` for field layouts
- Reference: `azerothcore-wotlk/src/server/game/DataStores/DBCStructure.h` for server-side struct definitions

## Requirements

### Wrapper Pattern
Each DBC wrapper should:
1. Inherit from or wrap the generic DBC reader
2. Define a struct with named fields matching the DBC columns
3. Provide `GetById(uint32 id)` lookup (most DBCs are keyed by first column)
4. Provide iteration support (`GetAll()`, `Num()`)
5. Cache parsed records on first access
6. Be a singleton or subsystem (one instance per DBC file)

### Priority DBC Tables

**Tier 1 — Needed by current specs:**

| DBC | Fields Needed | Used By |
|-----|--------------|---------|
| `Map.dbc` | ID, name, instanceType, mapType | World streaming |
| `AreaTable.dbc` | ID, mapID, parentAreaID, name, flags, ambientMultiplier | Zone detection |
| `Light.dbc` | ID, mapID, x/y/z, falloffStart/End, paramIDs[8] | Sky/atmosphere |
| `LightParams.dbc` | ID, highlightSky, skyboxID, glow, waterAlphas | Sky/atmosphere |
| `LightIntParams.dbc` | ID, values[18 time entries] | Sky/atmosphere |
| `LiquidType.dbc` | ID, name, flags, type, spellID | Water |
| `AnimationData.dbc` | ID, name, flags, fallback | M2 animation |
| `ChrRaces.dbc` | ID, name, clientPrefix, modelM/F | Character |
| `CharSections.dbc` | raceID, sexID, type, variation, color, textures[3] | Character |
| `CreatureDisplayInfo.dbc` | ID, modelID, soundID, extendedDisplayInfoID, scale, texture1/2/3 | Character/NPC |
| `CreatureModelData.dbc` | ID, modelPath, sizeClass, scale | Character/NPC |
| `ItemDisplayInfo.dbc` | ID, modelName[2], modelTexture[2], icon, geosetGroup[3] | Equipment |

**Tier 2 — Needed for gameplay:**

| DBC | Used By |
|-----|---------|
| `Spell.dbc` (234 fields!) | Combat, spellbook |
| `SpellVisual.dbc` | Spell effects |
| `SpellVisualKit.dbc` | Spell effects |
| `SoundEntries.dbc` | Audio |
| `LoadingScreens.dbc` | Loading screens |
| `GroundEffectTexture.dbc` | Ground clutter |
| `EmotesText.dbc` | Emote system |
| `Talent.dbc` + `TalentTab.dbc` | Talent UI |

### String Table
1. DBC files embed a string table at the end of the file
2. String fields are offsets into this table
3. Wrapper must resolve string offsets to `FString`

## Architecture

### File Structure
```
WowData/Public/Formats/Dbc/
├── DbcStore.h          — Singleton that loads/caches all DBC instances
├── MapDbc.h            — Map.dbc wrapper
├── AreaTableDbc.h      — AreaTable.dbc wrapper
├── LightDbc.h          — Light.dbc wrapper
├── LightParamsDbc.h    — LightParams.dbc wrapper
├── LightIntParamsDbc.h — LightIntParams.dbc wrapper
├── LiquidTypeDbc.h     — LiquidType.dbc wrapper
├── AnimationDataDbc.h  — AnimationData.dbc wrapper
├── ChrRacesDbc.h       — ChrRaces.dbc wrapper
├── CharSectionsDbc.h   — CharSections.dbc wrapper
├── CreatureDisplayInfoDbc.h
├── CreatureModelDataDbc.h
├── ItemDisplayInfoDbc.h
└── SpellDbc.h          — Spell.dbc (Tier 2)
```

### DbcStore (Central Registry)
```cpp
class FDbcStore {
    static FDbcStore& Get();
    void LoadAll(FMpqManager& Mpq);  // Load all registered DBCs

    FMapDbc& Maps();
    FAreaTableDbc& AreaTable();
    FLightDbc& Lights();
    // ... etc
};
```

## Acceptance Criteria
- [ ] Builds without errors
- [ ] All Tier 1 DBC tables parse without crashes
- [ ] `Map.dbc` returns correct map names (e.g., ID 0 = "Eastern Kingdoms")
- [ ] `Light.dbc` returns valid positions and param IDs
- [ ] `ChrRaces.dbc` returns race names and model prefixes
- [ ] String fields resolve correctly (not garbage text)
- [ ] Log output shows "Loaded X.dbc: N records" for each table

## Verification Steps
1. `./run_test.sh build`
2. `./run_test.sh` — check log for DBC load messages
3. Add debug logging that prints first 3 records from each DBC
4. Verify map names, race names, light positions are sensible
