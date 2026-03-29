#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SGridPanel.h"
#include "SlateCore.h"

class FWowInventoryManager;

class SWowBankItemSlot : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowBankItemSlot) {}
        SLATE_ARGUMENT(uint8, SlotIndex)
        SLATE_ARGUMENT(bool, bIsBagSlot)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FWowInventoryManager> InInventoryManager);

    virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

    void UpdateSlotDisplay();

private:
    const struct FWowItemSlot* GetItemSlot() const;
    FSlateColor GetSlotBorderColor() const;
    FSlateColor GetItemQualityColor() const;
    FText GetStackCountText() const;
    TSharedPtr<class SToolTip> CreateItemTooltip(const struct FWowItemSlot* ItemSlot) const;

    TSharedPtr<FWowInventoryManager> InventoryManager;
    uint8 SlotIndex = 0;
    bool bIsBagSlot = false;
    bool bIsHovered = false;
    TSharedPtr<class SBorder> SlotBorder;
    TSharedPtr<class STextBlock> StackCountText;
};

class WOWUI_API SWowBankWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowBankWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FWowInventoryManager> InInventoryManager);

    void Show();
    void Hide();
    bool IsBankVisible() const;

private:
    void OnInventoryChanged();
    FReply OnCloseClicked();
    FText GetBankBagStatusText() const;

    TSharedPtr<FWowInventoryManager> InventoryManager;
    TArray<TSharedPtr<SWowBankItemSlot>> BankSlots;
    TArray<TSharedPtr<SWowBankItemSlot>> BankBagSlots;
    EVisibility CurrentVisibility = EVisibility::Hidden;
};
