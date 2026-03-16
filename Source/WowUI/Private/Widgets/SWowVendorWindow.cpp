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

            // Item list (scrollable)
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(0.0f, 8.0f)
            [
                SAssignNew(ItemList, SScrollBox)
                .Orientation(Orient_Vertical)
                .ScrollBarAlwaysVisible(false)
            ]
        ]
    ];

    SetVisibility(EVisibility::Collapsed);
}

void SWowVendorWindow::UpdateVendor(uint64 VendorGuid, const TArray<FWowVendorItem>& Items)
{
    CurrentVendorGuid = VendorGuid;
    CurrentItems = Items;

    // Clear existing items
    ItemList->ClearChildren();

    // Add items to list
    for (const FWowVendorItem& Item : Items)
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