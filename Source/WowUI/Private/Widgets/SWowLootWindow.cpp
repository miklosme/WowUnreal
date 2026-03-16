#include "Widgets/SWowLootWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowLootWindow, Log, All);

void SWowLootWindow::Construct(const FArguments& InArgs)
{
    OnLootItem = InArgs._OnLootItem;
    OnLootAll = InArgs._OnLootAll;
    OnCloseLoot = InArgs._OnCloseLoot;

    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.95f)) // Dark background
        .BorderImage(FCoreStyle::Get().GetBrush("Border"))
        .Padding(8.0f)
        [
            SNew(SVerticalBox)

            // Title bar with close button
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Loot")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                    .ColorAndOpacity(FLinearColor::Yellow)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("X")))
                    .OnClicked(this, &SWowLootWindow::OnCloseClicked)
                ]
            ]

            // Gold display
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                .Padding(FMargin(0.0f, 4.0f))
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]() { return GetGoldText(CurrentGold); })
                    .ColorAndOpacity(FLinearColor::Yellow)
                    .Visibility_Lambda([this]() { return CurrentGold > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
                ]
            ]

            // Item list
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(ItemList, SVerticalBox)
            ]

            // Loot All button
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                .Padding(FMargin(0.0f, 4.0f))
                [
                    SAssignNew(LootAllButton, SButton)
                    .Text(FText::FromString(TEXT("Loot All")))
                    .HAlign(HAlign_Center)
                    .OnClicked(this, &SWowLootWindow::OnLootAllClicked)
                    .Visibility_Lambda([this]() { return CurrentItems.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
                ]
            ]
        ]
    ];

    SetVisibility(EVisibility::Collapsed);
}

void SWowLootWindow::UpdateLoot(uint64 LootGuid, uint8 LootType, uint32 Gold, const TArray<FWowLootItem>& Items)
{
    CurrentLootGuid = LootGuid;
    CurrentGold = Gold;
    CurrentItems = Items;

    // Clear existing items
    ItemList->ClearChildren();

    // Add items to list
    for (const FWowLootItem& Item : Items)
    {
        if (Item.bLooted) continue; // Skip already looted items

        FLinearColor QualityColor = FLinearColor::White;
        switch (Item.Quality)
        {
            case 0: QualityColor = FLinearColor::Gray; break;      // Poor (gray)
            case 1: QualityColor = FLinearColor::White; break;     // Common (white)
            case 2: QualityColor = FLinearColor::Green; break;     // Uncommon (green)
            case 3: QualityColor = FLinearColor::Blue; break;      // Rare (blue)
            case 4: QualityColor = FLinearColor(0.64f, 0.21f, 0.93f); break; // Epic (purple)
            case 5: QualityColor = FLinearColor(1.0f, 0.5f, 0.0f); break; // Legendary (orange)
        }

        ItemList->AddSlot()
        .AutoHeight()
        [
            SNew(SButton)
            .OnClicked_Lambda([this, Item]() { return OnLootItemClicked(Item.Index); })
            [
                SNew(SHorizontalBox)

                // Icon placeholder
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SBox)
                    .WidthOverride(32.0f)
                    .HeightOverride(32.0f)
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(QualityColor)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("?")))
                            .Justification(ETextJustify::Center)
                        ]
                    ]
                ]

                // Item name and count
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("Item %u"), Item.ItemId)))
                    .ColorAndOpacity(QualityColor)
                ]

                // Count
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("x%u"), Item.Count)))
                    .ColorAndOpacity(FLinearColor::White)
                    .Visibility_Lambda([Item]() { return Item.Count > 1 ? EVisibility::Visible : EVisibility::Collapsed; })
                ]
            ]
        ];
    }

    SetVisibility(EVisibility::Visible);
    UE_LOG(LogWowLootWindow, Log, TEXT("Updated loot window: %u gold, %d items"), Gold, Items.Num());
}

void SWowLootWindow::CloseLoot()
{
    CurrentLootGuid = 0;
    CurrentGold = 0;
    CurrentItems.Empty();
    ItemList->ClearChildren();
    SetVisibility(EVisibility::Collapsed);

    UE_LOG(LogWowLootWindow, Log, TEXT("Closed loot window"));
}

FReply SWowLootWindow::OnLootAllClicked()
{
    if (OnLootAll.IsBound())
    {
        OnLootAll.Execute();
    }
    return FReply::Handled();
}

FReply SWowLootWindow::OnCloseClicked()
{
    CloseLoot();
    if (OnCloseLoot.IsBound())
    {
        OnCloseLoot.Execute();
    }
    return FReply::Handled();
}

FReply SWowLootWindow::OnLootItemClicked(int32 LootSlot)
{
    if (OnLootItem.IsBound())
    {
        OnLootItem.Execute(LootSlot);
    }

    // Mark item as looted in our local copy
    for (FWowLootItem& Item : CurrentItems)
    {
        if (Item.Index == LootSlot)
        {
            Item.bLooted = true;
            break;
        }
    }

    // Refresh the item list
    UpdateLoot(CurrentLootGuid, 0, CurrentGold, CurrentItems);

    return FReply::Handled();
}

FText SWowLootWindow::GetGoldText(uint32 Gold) const
{
    if (Gold == 0)
    {
        return FText::GetEmpty();
    }

    uint32 GoldCoins = Gold / 10000;
    uint32 SilverCoins = (Gold % 10000) / 100;
    uint32 CopperCoins = Gold % 100;

    FString GoldText;
    if (GoldCoins > 0)
    {
        GoldText += FString::Printf(TEXT("%ug "), GoldCoins);
    }
    if (SilverCoins > 0)
    {
        GoldText += FString::Printf(TEXT("%us "), SilverCoins);
    }
    if (CopperCoins > 0 || Gold < 100)
    {
        GoldText += FString::Printf(TEXT("%uc"), CopperCoins);
    }

    return FText::FromString(GoldText.TrimEnd());
}