#pragma once
#include "CoreMinimal.h"
#include "WowEntity.h"

DECLARE_MULTICAST_DELEGATE(FOnInventoryChanged);

// Item slot information
struct FWowItemSlot
{
    uint64 ItemGuid = 0;
    uint32 ItemEntry = 0;
    uint32 StackCount = 0;
    uint8 Quality = 0; // 0=gray, 1=white, 2=green, 3=blue, 4=purple, 5=orange
    uint8 Bag = 255;   // 255 = backpack
    uint8 Slot = 0;

    bool IsEmpty() const { return ItemGuid == 0; }
};

// WoW inventory manager for tracking player items
class WOWUI_API FWowInventoryManager
{
public:
    FWowInventoryManager();

    /** Update inventory from player entity fields */
    void UpdateFromPlayerEntity(const FWowPlayerEntity& Player, class FWowEntityManager& EntityManager);

    /** Get item in backpack slot (0-15) */
    const FWowItemSlot* GetBackpackItem(uint8 Slot) const;

    /** Get equipped item by slot */
    const FWowItemSlot* GetEquippedItem(uint8 Slot) const;

    /** Get all backpack items */
    const TArray<FWowItemSlot>& GetBackpackItems() const { return BackpackItems; }

    /** Get all equipped items */
    const TArray<FWowItemSlot>& GetEquippedItems() const { return EquippedItems; }

    /** Send autoequip item packet */
    void AutoEquipItem(uint8 SrcBag, uint8 SrcSlot);

    /** Equipment slot constants */
    static constexpr uint8 EQUIP_SLOT_HEAD = 0;
    static constexpr uint8 EQUIP_SLOT_NECK = 1;
    static constexpr uint8 EQUIP_SLOT_SHOULDERS = 2;
    static constexpr uint8 EQUIP_SLOT_SHIRT = 3;
    static constexpr uint8 EQUIP_SLOT_CHEST = 4;
    static constexpr uint8 EQUIP_SLOT_WAIST = 5;
    static constexpr uint8 EQUIP_SLOT_LEGS = 6;
    static constexpr uint8 EQUIP_SLOT_FEET = 7;
    static constexpr uint8 EQUIP_SLOT_WRISTS = 8;
    static constexpr uint8 EQUIP_SLOT_HANDS = 9;
    static constexpr uint8 EQUIP_SLOT_FINGER1 = 10;
    static constexpr uint8 EQUIP_SLOT_FINGER2 = 11;
    static constexpr uint8 EQUIP_SLOT_TRINKET1 = 12;
    static constexpr uint8 EQUIP_SLOT_TRINKET2 = 13;
    static constexpr uint8 EQUIP_SLOT_BACK = 14;
    static constexpr uint8 EQUIP_SLOT_MAINHAND = 15;
    static constexpr uint8 EQUIP_SLOT_OFFHAND = 16;
    static constexpr uint8 EQUIP_SLOT_RANGED = 17;
    static constexpr uint8 EQUIP_SLOT_TABARD = 18;
    static constexpr uint8 EQUIP_SLOT_COUNT = 19;

    /** Get slot name for display */
    static const TCHAR* GetSlotName(uint8 SlotIndex);

    /** Get item quality color */
    static FLinearColor GetQualityColor(uint8 Quality);

    /** Event fired when inventory changes */
    FOnInventoryChanged OnInventoryChanged;

private:
    TArray<FWowItemSlot> BackpackItems; // 16 slots
    TArray<FWowItemSlot> EquippedItems; // 19 equipment slots

    void UpdateItemSlot(FWowItemSlot& Slot, uint64 ItemGuid, class FWowEntityManager& EntityManager, uint8 Bag, uint8 SlotIndex);
};