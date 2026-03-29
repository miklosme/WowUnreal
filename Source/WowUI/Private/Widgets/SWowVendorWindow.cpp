#include "Widgets/SWowVendorWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowVendorWindow, Log, All);

void SWowVendorWindow::Construct(const FArguments& InArgs)
{
    OnBuyItem = InArgs._OnBuyItem;
    OnSellItem = InArgs._OnSellItem;
    OnCloseVendor = InArgs._OnCloseVendor;

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
                    .Text(FText::FromString(TEXT("Vendor")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                    .ColorAndOpacity(FLinearColor::Yellow)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("X")))
                    .OnClicked(this, &SWowVendorWindow::OnCloseClicked)
                ]
            ]

            // Player money display
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f)
            [
                SAssignNew(PlayerMoneyText, STextBlock)
                .Text(FText::FromString(TEXT("0g 0s 0c")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                .ColorAndOpacity(FLinearColor::Yellow)
            ]

            // Buy/Sell tabs
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Buy")))
                    .OnClicked_Lambda([this]()
                    {
                        OnTabChanged(0);
                        return FReply::Handled();
                    })
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Sell")))
                    .OnClicked_Lambda([this]()
                    {
                        OnTabChanged(1);
                        return FReply::Handled();
                    })
                ]
            ]

            // Buy items list (scrollable)
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(0.0f, 8.0f)
            [
                SAssignNew(ItemList, SScrollBox)
                .Orientation(Orient_Vertical)
                .ScrollBarAlwaysVisible(false)
            ]

            // Sell items list (scrollable, initially hidden)
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(0.0f, 8.0f)
            [
                SAssignNew(SellItemList, SScrollBox)
                .Orientation(Orient_Vertical)
                .ScrollBarAlwaysVisible(false)
                .Visibility(EVisibility::Collapsed)
            ]
        ]
    ];

    SetVisibility(EVisibility::Collapsed);
}

void SWowVendorWindow::UpdateVendor(uint64 VendorGuid, const TArray<FWowVendorItem>& Items)
{
    CurrentVendorGuid = VendorGuid;
    CurrentItems = Items;

    // Reset to buy tab
    CurrentTab = 0;
    OnTabChanged(0);

    RefreshBuyList();

    SetVisibility(EVisibility::Visible);
    UE_LOG(LogWowVendorWindow, Log, TEXT("Updated vendor window: %d items"), Items.Num());
}

void SWowVendorWindow::CloseVendor()
{
    CurrentVendorGuid = 0;
    CurrentItems.Empty();
    ItemList->ClearChildren();
    SetVisibility(EVisibility::Collapsed);

    UE_LOG(LogWowVendorWindow, Log, TEXT("Closed vendor window"));
}

FReply SWowVendorWindow::OnCloseClicked()
{
    CloseVendor();
    if (OnCloseVendor.IsBound())
    {
        OnCloseVendor.Execute();
    }
    return FReply::Handled();
}

FReply SWowVendorWindow::OnBuyItemClicked(uint32 ItemId, int32 Count)
{
    if (OnBuyItem.IsBound())
    {
        OnBuyItem.Execute(CurrentVendorGuid, ItemId, Count);
        UE_LOG(LogWowVendorWindow, Log, TEXT("Buy item %u (count=%d) from vendor %llu"), ItemId, Count, CurrentVendorGuid);
    }
    return FReply::Handled();
}

void SWowVendorWindow::UpdatePlayerInventory(const TArray<FWowItem>& Items)
{
    PlayerInventoryItems = Items;
    RefreshSellList();

    // Update money display
    if (PlayerMoneyText.IsValid())
    {
        PlayerMoneyText->SetText(GetPriceText(PlayerMoney));
    }
}

void SWowVendorWindow::OnTabChanged(int32 TabIndex)
{
    CurrentTab = TabIndex;

    if (TabIndex == 0) // Buy tab
    {
        if (ItemList.IsValid())
        {
            ItemList->SetVisibility(EVisibility::Visible);
        }
        if (SellItemList.IsValid())
        {
            SellItemList->SetVisibility(EVisibility::Collapsed);
        }
    }
    else // Sell tab
    {
        if (ItemList.IsValid())
        {
            ItemList->SetVisibility(EVisibility::Collapsed);
        }
        if (SellItemList.IsValid())
        {
            SellItemList->SetVisibility(EVisibility::Visible);
        }
        RefreshSellList();
    }
}

void SWowVendorWindow::RefreshBuyList()
{
    if (!ItemList.IsValid()) return;

    ItemList->ClearChildren();

    for (const FWowVendorItem& Item : CurrentItems)
    {
        ItemList->AddSlot()
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.8f))
            .Padding(8.0f)
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
                        .BorderBackgroundColor(FLinearColor::White)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("?")))
                            .Justification(ETextJustify::Center)
                        ]
                    ]
                ]

                // Item details
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("Item %u"), Item.ItemId)))
                        .ColorAndOpacity(FLinearColor::White)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(GetPriceText(Item.Price))
                        .ColorAndOpacity(FLinearColor::Yellow)
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    ]
                ]

                // Stock info
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(FString::Printf(TEXT("Stock: %u"), Item.MaxCount)))
                    .ColorAndOpacity(FLinearColor::Gray)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    .Visibility_Lambda([Item]() { return Item.MaxCount > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
                ]

                // Buy button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Buy")))
                    .OnClicked_Lambda([this, Item]() { return OnBuyItemClicked(Item.ItemId, 1); })
                ]
            ]
        ];
    }
}

void SWowVendorWindow::RefreshSellList()
{
    if (!SellItemList.IsValid()) return;

    SellItemList->ClearChildren();

    for (const FWowItem& Item : PlayerInventoryItems)
    {
        // Skip soulbound items or items that can't be sold (quality checks, etc.)
        if (Item.Quality > 0) // Only sell grey items for simplicity
        {
            continue;
        }

        SellItemList->AddSlot()
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(0.2f, 0.2f, 0.2f, 0.8f))
            .Padding(8.0f)
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
                        .BorderBackgroundColor(FLinearColor::Gray)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("?")))
                            .Justification(ETextJustify::Center)
                        ]
                    ]
                ]

                // Item details
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("Item %u"), Item.Entry)))
                        .ColorAndOpacity(FLinearColor::White)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("Count: %u"), Item.Count)))
                        .ColorAndOpacity(FLinearColor::Gray)
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    ]
                ]

                // Sell button
                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Center)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Sell")))
                    .OnClicked_Lambda([this, Item]() { return OnSellItemClicked(Item.Guid, 1); })
                ]
            ]
        ];
    }
}

FReply SWowVendorWindow::OnSellItemClicked(uint64 ItemGuid, uint8 Count)
{
    if (OnSellItem.IsBound())
    {
        OnSellItem.Execute(CurrentVendorGuid, ItemGuid, Count);
        UE_LOG(LogWowVendorWindow, Log, TEXT("Sell item %llu (count=%d) to vendor %llu"), ItemGuid, Count, CurrentVendorGuid);
    }
    return FReply::Handled();
}

FText SWowVendorWindow::GetPriceText(uint32 Price) const
{
    if (Price == 0)
    {
        return FText::FromString(TEXT("Free"));
    }

    uint32 GoldCoins = Price / 10000;
    uint32 SilverCoins = (Price % 10000) / 100;
    uint32 CopperCoins = Price % 100;

    FString PriceText;
    if (GoldCoins > 0)
    {
        PriceText += FString::Printf(TEXT("%ug "), GoldCoins);
    }
    if (SilverCoins > 0)
    {
        PriceText += FString::Printf(TEXT("%us "), SilverCoins);
    }
    if (CopperCoins > 0 || Price < 100)
    {
        PriceText += FString::Printf(TEXT("%uc"), CopperCoins);
    }

    return FText::FromString(PriceText.TrimEnd());
}