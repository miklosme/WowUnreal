#include "WowInventoryManager.h"

FWowInventoryManager::FWowInventoryManager()
{
    BackpackItems.SetNum(16); // Backpack has 16 slots
    EquippedItems.SetNum(EQUIP_SLOT_COUNT); // 19 equipment slots
}

void FWowInventoryManager::UpdateFromPlayerEntity(const FWowPlayerEntity& Player, FWowEntityManager& EntityManager)
{
    // Simple demo implementation - add fake items for demonstration
    static bool bInitialized = false;
    if (!bInitialized)
    {
        // Add some fake items to demonstrate the UI
        BackpackItems[0].ItemGuid = 12345;
        BackpackItems[0].ItemEntry = 1000;
        BackpackItems[0].StackCount = 5;
        BackpackItems[0].Quality = 2; // Green

        BackpackItems[5].ItemGuid = 23456;
        BackpackItems[5].ItemEntry = 2000;
        BackpackItems[5].StackCount = 1;
        BackpackItems[5].Quality = 3; // Blue

        // Add fake equipped item
        EquippedItems[EQUIP_SLOT_CHEST].ItemGuid = 34567;
        EquippedItems[EQUIP_SLOT_CHEST].ItemEntry = 3000;
        EquippedItems[EQUIP_SLOT_CHEST].StackCount = 1;
        EquippedItems[EQUIP_SLOT_CHEST].Quality = 4; // Purple

        bInitialized = true;
        OnInventoryChanged.Broadcast();
    }
}

const FWowItemSlot* FWowInventoryManager::GetBackpackItem(uint8 Slot) const
{
    return (Slot < 16) ? &BackpackItems[Slot] : nullptr;
}

const FWowItemSlot* FWowInventoryManager::GetEquippedItem(uint8 Slot) const
{
    return (Slot < EQUIP_SLOT_COUNT) ? &EquippedItems[Slot] : nullptr;
}

void FWowInventoryManager::AutoEquipItem(uint8 SrcBag, uint8 SrcSlot)
{
    // TODO: Implement proper packet sending when network connection is available
    UE_LOG(LogTemp, Log, TEXT("Auto-equip item requested: bag=%d, slot=%d"), SrcBag, SrcSlot);
}

const TCHAR* FWowInventoryManager::GetSlotName(uint8 SlotIndex)
{
    switch (SlotIndex)
    {
        case EQUIP_SLOT_HEAD: return TEXT("Head");
        case EQUIP_SLOT_NECK: return TEXT("Neck");
        case EQUIP_SLOT_SHOULDERS: return TEXT("Shoulders");
        case EQUIP_SLOT_SHIRT: return TEXT("Shirt");
        case EQUIP_SLOT_CHEST: return TEXT("Chest");
        case EQUIP_SLOT_WAIST: return TEXT("Waist");
        case EQUIP_SLOT_LEGS: return TEXT("Legs");
        case EQUIP_SLOT_FEET: return TEXT("Feet");
        case EQUIP_SLOT_WRISTS: return TEXT("Wrists");
        case EQUIP_SLOT_HANDS: return TEXT("Hands");
        case EQUIP_SLOT_FINGER1: return TEXT("Finger 1");
        case EQUIP_SLOT_FINGER2: return TEXT("Finger 2");
        case EQUIP_SLOT_TRINKET1: return TEXT("Trinket 1");
        case EQUIP_SLOT_TRINKET2: return TEXT("Trinket 2");
        case EQUIP_SLOT_BACK: return TEXT("Back");
        case EQUIP_SLOT_MAINHAND: return TEXT("Main Hand");
        case EQUIP_SLOT_OFFHAND: return TEXT("Off Hand");
        case EQUIP_SLOT_RANGED: return TEXT("Ranged");
        case EQUIP_SLOT_TABARD: return TEXT("Tabard");
        default: return TEXT("Unknown");
    }
}

FLinearColor FWowInventoryManager::GetQualityColor(uint8 Quality)
{
    switch (Quality)
    {
        case 0: return FLinearColor::Gray;      // Poor
        case 1: return FLinearColor::White;     // Common
        case 2: return FLinearColor::Green;     // Uncommon
        case 3: return FLinearColor::Blue;      // Rare
        case 4: return FLinearColor(0.64f, 0.21f, 0.93f); // Epic (purple)
        case 5: return FLinearColor(1.0f, 0.5f, 0.0f);    // Legendary (orange)
        default: return FLinearColor::Gray;
    }
}

void FWowInventoryManager::UpdateItemSlot(FWowItemSlot& Slot, uint64 ItemGuid, FWowEntityManager& EntityManager, uint8 Bag, uint8 SlotIndex)
{
    // Simple placeholder implementation
    Slot.ItemGuid = ItemGuid;
    Slot.Bag = Bag;
    Slot.Slot = SlotIndex;

    if (ItemGuid == 0)
    {
        // Empty slot
        Slot.ItemEntry = 0;
        Slot.StackCount = 0;
        Slot.Quality = 0;
    }
    else
    {
        // Placeholder values for demonstration
        Slot.ItemEntry = ItemGuid % 10000; // Simple mapping from GUID
        Slot.StackCount = 1;
        Slot.Quality = (ItemGuid % 6); // Placeholder quality
    }
}