#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "SlateCore.h"

class FWowInventoryManager;

// Single equipment slot widget
class SWowEquipmentSlot : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowEquipmentSlot) {}
        SLATE_ARGUMENT(uint8, SlotIndex)
        SLATE_ARGUMENT(FString, SlotName)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FWowInventoryManager> InInventoryManager);

    // SWidget overrides
    virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

    /** Update the slot display */
    void UpdateSlotDisplay();

private:

    TSharedPtr<FWowInventoryManager> InventoryManager;
    uint8 SlotIndex = 0;
    FString SlotName;
    TSharedPtr<class SBorder> SlotBorder;
    bool bIsHovered = false;

    FSlateColor GetSlotBorderColor() const;
    FSlateColor GetItemQualityColor() const;
    FText GetSlotText() const;
};

// Main character panel widget
class WOWUI_API SWowCharacterPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowCharacterPanel) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, TSharedPtr<FWowInventoryManager> InInventoryManager);

    /** Show/hide the character panel */
    void SetPanelVisibility(EVisibility InVisibility);

    /** Toggle panel visibility */
    void ToggleVisibility();

    /** Check if panel is visible */
    bool IsPanelVisible() const;

private:
    void OnInventoryChanged();

    TSharedPtr<FWowInventoryManager> InventoryManager;
    TArray<TSharedPtr<SWowEquipmentSlot>> EquipmentSlots;
    EVisibility CurrentVisibility = EVisibility::Hidden;
};