#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SGridPanel.h"
#include "SlateCore.h"

class FWowInventoryManager;

// Single item slot widget for the bag
class SWowItemSlot : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowItemSlot) {}
        SLATE_ARGUMENT(uint8, SlotIndex)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FWowInventoryManager> InInventoryManager);

    // SWidget overrides
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

    /** Update the slot display */
    void UpdateSlotDisplay();

private:

    TSharedPtr<FWowInventoryManager> InventoryManager;
    uint8 SlotIndex = 0;
    TSharedPtr<class SBorder> SlotBorder;
    TSharedPtr<class STextBlock> StackCountText;
    bool bIsHovered = false;

    FSlateColor GetSlotBorderColor() const;
    FSlateColor GetItemQualityColor() const;
    FText GetStackCountText() const;
};

// Main bag window widget
class WOWUI_API SWowBagWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowBagWindow) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FWowInventoryManager> InInventoryManager);

    /** Show/hide the bag window */
    void SetBagVisibility(EVisibility InVisibility);

    /** Toggle bag visibility */
    void ToggleVisibility();

    /** Check if bag is visible */
    bool IsBagVisible() const;

private:
    void OnInventoryChanged();

    TSharedPtr<FWowInventoryManager> InventoryManager;
    TSharedPtr<SGridPanel> ItemGrid;
    TArray<TSharedPtr<SWowItemSlot>> ItemSlots;
    EVisibility CurrentVisibility = EVisibility::Hidden;
};