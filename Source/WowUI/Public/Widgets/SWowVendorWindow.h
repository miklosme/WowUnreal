#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "WowEntity.h"

class SScrollBox;
class SButton;

DECLARE_DELEGATE_ThreeParams(FOnBuyItem, uint64 /*VendorGuid*/, uint32 /*ItemId*/, int32 /*Count*/);
DECLARE_DELEGATE_ThreeParams(FOnSellItem, uint64 /*VendorGuid*/, uint64 /*ItemGuid*/, uint8 /*Count*/);
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
        SLATE_EVENT(FOnSellItem, OnSellItem)
        SLATE_EVENT(FOnCloseVendor, OnCloseVendor)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Update the vendor window with new data */
    void UpdateVendor(uint64 VendorGuid, const TArray<FWowVendorItem>& Items);

    /** Update player inventory for sell tab */
    void UpdatePlayerInventory(const TArray<FWowItem>& Items);

    /** Hide the vendor window */
    void CloseVendor();

private:
    FOnBuyItem OnBuyItem;
    FOnSellItem OnSellItem;
    FOnCloseVendor OnCloseVendor;

    TSharedPtr<SScrollBox> ItemList;
    TSharedPtr<class STabView> TabView;
    TSharedPtr<SScrollBox> SellItemList;
    TSharedPtr<class STextBlock> PlayerMoneyText;

    uint64 CurrentVendorGuid = 0;
    TArray<FWowVendorItem> CurrentItems;
    TArray<FWowItem> PlayerInventoryItems;
    uint32 PlayerMoney = 0;

    /** Current tab: 0=Buy, 1=Sell */
    int32 CurrentTab = 0;

    FReply OnCloseClicked();
    FReply OnBuyItemClicked(uint32 ItemId, int32 Count);
    FReply OnSellItemClicked(uint64 ItemGuid, uint8 Count);

    /** Handle tab switching */
    void OnTabChanged(int32 TabIndex);

    /** Generate a WoW-style price display text (123g 45s 67c) */
    FText GetPriceText(uint32 Price) const;

    /** Update buy/sell lists */
    void RefreshBuyList();
    void RefreshSellList();
};