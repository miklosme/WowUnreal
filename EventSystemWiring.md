# WoW Event System Wiring Summary

This document summarizes the WoW event system wiring that has been implemented to fire Lua events that FrameXML scripts depend on.

## Helper Method Added

**File**: `WowGameplayController.h` / `WowGameplayController.cpp`
- Added `FireUIEvent(const FString& EventName, const TArray<FString>& Args = {})` helper method
- This method safely checks for UIManager and EventSystem availability before firing events

## Events Wired Up

### 1. Health/Mana Changes (OnEntityUpdated)
**Location**: `WowGameplayController::OnEntityUpdated`
- **UNIT_HEALTH** with arg "player" - when local player health changes
- **UNIT_MANA** with arg "player" - when local player mana changes
- **UNIT_HEALTH** with arg "target" - when current target health changes
- **UNIT_MANA** with arg "target" - when current target mana changes

### 2. Target Changes (SetTarget)
**Location**: `WowGameplayController::SetTarget`
- **PLAYER_TARGET_CHANGED** - fired when target changes

### 3. Spell Casting Events
**Location**: Various spell event handlers
- **UNIT_SPELLCAST_START** with arg "player" + **CURRENT_SPELL_CAST_CHANGED** - in `OnSpellStart`
- **UNIT_SPELLCAST_SUCCEEDED** + **SPELL_UPDATE_COOLDOWN** + **ACTIONBAR_UPDATE_COOLDOWN** - in `OnSpellGo`
- **UNIT_SPELLCAST_FAILED** with arg "player" - in `OnSpellFailure`

### 4. Action Bar Usage (CastSpellFromSlot)
**Location**: `WowGameplayController::CastSpellFromSlot`
- **ACTIONBAR_UPDATE_STATE** - when action bar button is used

### 5. Combat Damage (OnAttackerStateUpdate)
**Location**: `WowGameplayController::OnAttackerStateUpdate`
- **COMBAT_LOG_EVENT** - fired for all combat damage events

### 6. Target Death (OnEntityHealthChanged)
**Location**: `WowGameplayController::OnEntityHealthChanged`
- **UNIT_HEALTH** with arg "target" - when current target dies

### 7. Chat Messages (OnChatMessage)
**Location**: `WowGameplayController::OnChatMessage`
- **CHAT_MSG_SAY** - chat type 0
- **CHAT_MSG_PARTY** - chat type 1
- **CHAT_MSG_RAID** - chat type 2
- **CHAT_MSG_GUILD** - chat type 3
- **CHAT_MSG_YELL** - chat type 6
- **CHAT_MSG_WHISPER** - chat type 7

### 8. Inventory Updates (OnPlayerInventoryUpdated)
**Location**: `WowGameplayController::OnPlayerInventoryUpdated`
- **BAG_UPDATE** - when inventory changes
- **PLAYER_MONEY** - when player money changes

### 9. Spell Book Updates (OnInitialSpells) - NEW DELEGATE
**Location**: `WowGameplayController::OnInitialSpells`
- **SPELLS_CHANGED** - when initial spells are loaded
- **LEARNED_SPELL_IN_TAB** - when spells are learned

### 10. Action Bar Data Updates (OnActionButtonsUpdated) - NEW DELEGATE
**Location**: `WowGameplayController::OnActionButtonsUpdated`
- **ACTIONBAR_SLOT_CHANGED** with slot index - for each of the 12 action bar slots
- **ACTIONBAR_UPDATE_STATE** - general action bar state update

## New Delegates Added

**File**: `WowPacketHandler.h`
```cpp
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInitialSpells, const TArray<uint32>& /*SpellIds*/);
DECLARE_MULTICAST_DELEGATE(FOnActionButtonsUpdated);
```

**Member Variables Added**:
```cpp
FOnInitialSpells OnInitialSpells;
FOnActionButtonsUpdated OnActionButtonsUpdated;
```

**Handler Modifications**:
- `FWowPacketHandler::HandleInitialSpells` - now broadcasts `OnInitialSpells` with spell list
- `FWowPacketHandler::HandleActionButtons` - now broadcasts `OnActionButtonsUpdated`

**Bindings Added** in `WowGameplayController::BeginPlay`:
```cpp
ConnectionManager->PacketHandler.OnInitialSpells.AddUObject(
    this, &AWowGameplayController::OnInitialSpells);
ConnectionManager->PacketHandler.OnActionButtonsUpdated.AddUObject(
    this, &AWowGameplayController::OnActionButtonsUpdated);
```

## Technical Details

### Mana Handling
- Uses `Entity.GetPower(0)` where 0 = mana power type
- Only tracks mana for unit entities (checked with `Entity.IsUnit()`)
- Casts to `FWowUnitEntity*` to access `GetPower` method

### Event Arguments
- Most events use simple string arguments or no arguments
- FrameXML handlers can query unit state via API calls when they receive events
- Event names match standard WoW 3.3.5 UI event names

## Compilation Status

- Core event system changes compile successfully
- WowGameplayController.cpp compiles without errors
- WowPacketHandler.cpp compiles without errors
- Some unrelated UI files have compilation issues but don't affect event system functionality

## Usage

FrameXML scripts can now register for these events using:
```lua
frame:RegisterEvent("UNIT_HEALTH")
frame:RegisterEvent("PLAYER_TARGET_CHANGED")
-- etc.
```

The events will be fired automatically as gameplay state changes occur.