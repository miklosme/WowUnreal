#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WowEntity.h"
#include "WowGameUI.generated.h"

class SWowLootWindow;
class SWowVendorWindow;
class SWowQuestDialog;
class UWowConnectionManager;

/**
 * Game UI manager that handles WoW-style UI windows
 * Integrates with the connection manager for packet sending
 */
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class WOWUI_API UWowGameUI : public UActorComponent
{
    GENERATED_BODY()

public:
    UWowGameUI();

    virtual void BeginPlay() override;

    /** Set the connection manager for sending packets */
    UFUNCTION(BlueprintCallable)
    void SetConnectionManager(UWowConnectionManager* InConnectionManager);

    /** Show the loot window with the given data */
    void ShowLootWindow(uint64 LootGuid, uint8 LootType, uint32 Gold, const TArray<FWowLootItem>& Items);

    /** Hide the loot window */
    void HideLootWindow();

    /** Show the vendor window with the given data */
    void ShowVendorWindow(uint64 VendorGuid, const TArray<FWowVendorItem>& Items);

    /** Update vendor window with player inventory for sell functionality */
    void UpdateVendorPlayerInventory(const TArray<FWowItem>& Items);

    /** Hide the vendor window */
    void HideVendorWindow();

    /** Show quest details dialog */
    void ShowQuestDialog(const FWowQuestDetails& QuestDetails);

    /** Show quest reward dialog */
    void ShowQuestRewardDialog(const FWowQuestDetails& QuestDetails);

    /** Hide quest dialog */
    void HideQuestDialog();

protected:
    UPROPERTY()
    TObjectPtr<UWowConnectionManager> ConnectionManager;

private:
    // Widget instances
    TSharedPtr<SWowLootWindow> LootWindow;
    TSharedPtr<SWowVendorWindow> VendorWindow;
    TSharedPtr<SWowQuestDialog> QuestDialog;

    // Widget callbacks
    void OnLootItem(int32 LootSlot);
    void OnLootAll();
    void OnCloseLoot();
    void OnBuyItem(uint64 VendorGuid, uint32 ItemId, int32 Count);
    void OnSellItem(uint64 VendorGuid, uint64 ItemGuid, uint8 Count);
    void OnCloseVendor();
    void OnQuestAccept(uint64 QuestGiverGuid, uint32 QuestId);
    void OnQuestComplete(uint64 QuestGiverGuid, uint32 QuestId, int32 RewardChoice);
    void OnQuestDecline();

    // UI integration
    void CreateWidgets();
    void AddWidgetsToViewport();
    void RemoveWidgetsFromViewport();

    bool bWidgetsCreated = false;
};