#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "WowEntity.h"

class SButton;
class SVerticalBox;

DECLARE_DELEGATE_OneParam(FOnLootItem, int32 /*LootSlot*/);
DECLARE_DELEGATE(FOnLootAll);
DECLARE_DELEGATE(FOnCloseLoot);

/**
 * WoW-style loot window widget
 * Dark panel with gold border, item list, Loot All button, and close button
 */
class WOWUI_API SWowLootWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowLootWindow) {}
        SLATE_EVENT(FOnLootItem, OnLootItem)
        SLATE_EVENT(FOnLootAll, OnLootAll)
        SLATE_EVENT(FOnCloseLoot, OnCloseLoot)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Update the loot window with new data */
    void UpdateLoot(uint64 LootGuid, uint8 LootType, uint32 Gold, const TArray<FWowLootItem>& Items);

    /** Hide the loot window */
    void CloseLoot();

private:
    FOnLootItem OnLootItem;
    FOnLootAll OnLootAll;
    FOnCloseLoot OnCloseLoot;

    TSharedPtr<SVerticalBox> ItemList;
    TSharedPtr<SButton> LootAllButton;

    uint64 CurrentLootGuid = 0;
    uint32 CurrentGold = 0;
    TArray<FWowLootItem> CurrentItems;

    FReply OnLootAllClicked();
    FReply OnCloseClicked();
    FReply OnLootItemClicked(int32 LootSlot);

    /** Generate a WoW-style gold display text (123g 45s 67c) */
    FText GetGoldText(uint32 Gold) const;
};