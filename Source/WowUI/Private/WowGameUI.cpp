#include "WowGameUI.h"
#include "Widgets/SWowLootWindow.h"
#include "Widgets/SWowVendorWindow.h"
#include "Widgets/SWowQuestDialog.h"
#include "WowConnectionManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Layout/SConstraintCanvas.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowGameUI, Log, All);

UWowGameUI::UWowGameUI()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UWowGameUI::BeginPlay()
{
    Super::BeginPlay();
    CreateWidgets();
}

void UWowGameUI::SetConnectionManager(UWowConnectionManager* InConnectionManager)
{
    ConnectionManager = InConnectionManager;
}

void UWowGameUI::ShowLootWindow(uint64 LootGuid, uint8 LootType, uint32 Gold, const TArray<FWowLootItem>& Items)
{
    CreateWidgets();
    if (LootWindow.IsValid())
    {
        LootWindow->UpdateLoot(LootGuid, LootType, Gold, Items);
        UE_LOG(LogWowGameUI, Log, TEXT("Showing loot window"));
    }
}

void UWowGameUI::HideLootWindow()
{
    if (LootWindow.IsValid())
    {
        LootWindow->CloseLoot();
        UE_LOG(LogWowGameUI, Log, TEXT("Hiding loot window"));
    }
}

void UWowGameUI::ShowVendorWindow(uint64 VendorGuid, const TArray<FWowVendorItem>& Items)
{
    CreateWidgets();
    if (VendorWindow.IsValid())
    {
        VendorWindow->UpdateVendor(VendorGuid, Items);
        UE_LOG(LogWowGameUI, Log, TEXT("Showing vendor window"));
    }
}

void UWowGameUI::UpdateVendorPlayerInventory(const TArray<FWowItem>& Items)
{
    if (VendorWindow.IsValid())
    {
        VendorWindow->UpdatePlayerInventory(Items);
        UE_LOG(LogWowGameUI, Log, TEXT("Updated vendor player inventory"));
    }
}

void UWowGameUI::HideVendorWindow()
{
    if (VendorWindow.IsValid())
    {
        VendorWindow->CloseVendor();
        UE_LOG(LogWowGameUI, Log, TEXT("Hiding vendor window"));
    }
}

void UWowGameUI::ShowQuestDialog(const FWowQuestDetails& QuestDetails)
{
    CreateWidgets();
    if (QuestDialog.IsValid())
    {
        QuestDialog->ShowQuestDetails(QuestDetails);
        UE_LOG(LogWowGameUI, Log, TEXT("Showing quest dialog"));
    }
}

void UWowGameUI::ShowQuestRewardDialog(const FWowQuestDetails& QuestDetails)
{
    CreateWidgets();
    if (QuestDialog.IsValid())
    {
        QuestDialog->ShowQuestReward(QuestDetails);
        UE_LOG(LogWowGameUI, Log, TEXT("Showing quest reward dialog"));
    }
}

void UWowGameUI::HideQuestDialog()
{
    if (QuestDialog.IsValid())
    {
        QuestDialog->CloseDialog();
        UE_LOG(LogWowGameUI, Log, TEXT("Hiding quest dialog"));
    }
}

void UWowGameUI::CreateWidgets()
{
    if (bWidgetsCreated) return;

    // Create loot window
    LootWindow = SNew(SWowLootWindow)
        .OnLootItem_UObject(this, &UWowGameUI::OnLootItem)
        .OnLootAll_UObject(this, &UWowGameUI::OnLootAll)
        .OnCloseLoot_UObject(this, &UWowGameUI::OnCloseLoot);

    // Create vendor window
    VendorWindow = SNew(SWowVendorWindow)
        .OnBuyItem_UObject(this, &UWowGameUI::OnBuyItem)
        .OnSellItem_UObject(this, &UWowGameUI::OnSellItem)
        .OnCloseVendor_UObject(this, &UWowGameUI::OnCloseVendor);

    // Create quest dialog
    QuestDialog = SNew(SWowQuestDialog)
        .OnQuestAccept_UObject(this, &UWowGameUI::OnQuestAccept)
        .OnQuestComplete_UObject(this, &UWowGameUI::OnQuestComplete)
        .OnQuestDecline_UObject(this, &UWowGameUI::OnQuestDecline);

    AddWidgetsToViewport();
    bWidgetsCreated = true;

    UE_LOG(LogWowGameUI, Log, TEXT("Created WoW game UI widgets"));
}

void UWowGameUI::AddWidgetsToViewport()
{
    if (!GEngine || !GEngine->GameViewport) return;

    // Add widgets to viewport with proper positioning
    GEngine->GameViewport->AddViewportWidgetContent(
        SNew(SConstraintCanvas)
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.5f, 0.3f, 0.5f, 0.3f))
        .Offset(FMargin(-200.0f, -150.0f, 200.0f, 150.0f))
        [
            LootWindow.ToSharedRef()
        ]
    );

    GEngine->GameViewport->AddViewportWidgetContent(
        SNew(SConstraintCanvas)
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.2f, 0.2f, 0.2f, 0.2f))
        .Offset(FMargin(-150.0f, -200.0f, 300.0f, 400.0f))
        [
            VendorWindow.ToSharedRef()
        ]
    );

    GEngine->GameViewport->AddViewportWidgetContent(
        SNew(SConstraintCanvas)
        + SConstraintCanvas::Slot()
        .Anchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f))
        .Offset(FMargin(-250.0f, -200.0f, 250.0f, 200.0f))
        [
            QuestDialog.ToSharedRef()
        ]
    );
}

void UWowGameUI::RemoveWidgetsFromViewport()
{
    if (GEngine && GEngine->GameViewport)
    {
        if (LootWindow.IsValid())
        {
            GEngine->GameViewport->RemoveViewportWidgetContent(LootWindow.ToSharedRef());
        }
        if (VendorWindow.IsValid())
        {
            GEngine->GameViewport->RemoveViewportWidgetContent(VendorWindow.ToSharedRef());
        }
        if (QuestDialog.IsValid())
        {
            GEngine->GameViewport->RemoveViewportWidgetContent(QuestDialog.ToSharedRef());
        }
    }
}

void UWowGameUI::OnLootItem(int32 LootSlot)
{
    if (ConnectionManager)
    {
        ConnectionManager->SendLootItem(LootSlot);
        UE_LOG(LogWowGameUI, Log, TEXT("Looting item slot %d"), LootSlot);
    }
}

void UWowGameUI::OnLootAll()
{
    // For loot all, we would need to iterate through all available loot slots
    // For now, just send loot for slot 0 as an example
    if (ConnectionManager)
    {
        ConnectionManager->SendLootItem(0);
        UE_LOG(LogWowGameUI, Log, TEXT("Loot all clicked"));
    }
}

void UWowGameUI::OnCloseLoot()
{
    UE_LOG(LogWowGameUI, Log, TEXT("Loot window closed by user"));
    // The loot window is already hidden by the widget itself
}

void UWowGameUI::OnBuyItem(uint64 VendorGuid, uint32 ItemId, int32 Count)
{
    if (ConnectionManager)
    {
        ConnectionManager->SendBuyItem(VendorGuid, ItemId, Count);
        UE_LOG(LogWowGameUI, Log, TEXT("Buying item %u (count=%d) from vendor %llu"), ItemId, Count, VendorGuid);
    }
}

void UWowGameUI::OnSellItem(uint64 VendorGuid, uint64 ItemGuid, uint8 Count)
{
    if (ConnectionManager)
    {
        ConnectionManager->SendSellItem(VendorGuid, ItemGuid, Count);
        UE_LOG(LogWowGameUI, Log, TEXT("Selling item %llu (count=%d) to vendor %llu"), ItemGuid, Count, VendorGuid);
    }
}

void UWowGameUI::OnCloseVendor()
{
    UE_LOG(LogWowGameUI, Log, TEXT("Vendor window closed by user"));
    // The vendor window is already hidden by the widget itself
}

void UWowGameUI::OnQuestAccept(uint64 QuestGiverGuid, uint32 QuestId)
{
    if (ConnectionManager)
    {
        ConnectionManager->SendQuestAccept(QuestGiverGuid, QuestId);
        UE_LOG(LogWowGameUI, Log, TEXT("Accepting quest %u from questgiver %llu"), QuestId, QuestGiverGuid);
    }
}

void UWowGameUI::OnQuestComplete(uint64 QuestGiverGuid, uint32 QuestId, int32 RewardChoice)
{
    if (ConnectionManager)
    {
        ConnectionManager->SendQuestChooseReward(QuestGiverGuid, QuestId, RewardChoice);
        UE_LOG(LogWowGameUI, Log, TEXT("Completing quest %u with reward choice %d from questgiver %llu"), QuestId, RewardChoice, QuestGiverGuid);
    }
}

void UWowGameUI::OnQuestDecline()
{
    UE_LOG(LogWowGameUI, Log, TEXT("Quest declined by user"));
    // No packet needs to be sent for quest decline - just close the dialog
}