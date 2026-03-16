#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "WowEntity.h"

class SScrollBox;
class SButton;

DECLARE_DELEGATE_ThreeParams(FOnBuyItem, uint64 /*VendorGuid*/, uint32 /*ItemId*/, int32 /*Count*/);
DECLARE_DELEGATE(FOnCloseVendor);

/**
 * WoW-style vendor/merchant window widget
 * Scrollable list of vendor items with prices and buy buttons
 */
class WOWUI_API SWowVendorWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowVendorWindow) {}
        SLATE_EVENT(FOnBuyItem, OnBuyItem)
        SLATE_EVENT(FOnCloseVendor, OnCloseVendor)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Update the vendor window with new data */
    void UpdateVendor(uint64 VendorGuid, const TArray<FWowVendorItem>& Items);

    /** Hide the vendor window */
    void CloseVendor();

private:
    FOnBuyItem OnBuyItem;
    FOnCloseVendor OnCloseVendor;

    TSharedPtr<SScrollBox> ItemList;

    uint64 CurrentVendorGuid = 0;
    TArray<FWowVendorItem> CurrentItems;

    FReply OnCloseClicked();
    FReply OnBuyItemClicked(uint32 ItemId, int32 Count);

    /** Generate a WoW-style price display text (123g 45s 67c) */
    FText GetPriceText(uint32 Price) const;
};