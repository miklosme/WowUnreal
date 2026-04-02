#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "SlateCore.h"

class FWowInventoryManager;
class UWowConnectionManager;

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
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
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

    void Construct(const FArguments& InArgs, TSharedPtr<FWowInventoryManager> InInventoryManager, UWowConnectionManager* InConnectionManager = nullptr);

    /** Show/hide the character panel */
    void SetPanelVisibility(EVisibility InVisibility);

    /** Toggle panel visibility */
    void ToggleVisibility();

    /** Check if panel is visible */
    bool IsPanelVisible() const;

    /** Update stats display (call when opening panel or stats change) */
    void UpdateStats();

    /** Set connection manager for stats access */
    void SetConnectionManager(UWowConnectionManager* InConnectionManager);

private:
    void OnInventoryChanged();

    TSharedPtr<FWowInventoryManager> InventoryManager;
    UWowConnectionManager* ConnectionManager;
    TArray<TSharedPtr<SWowEquipmentSlot>> EquipmentSlots;
    EVisibility CurrentVisibility = EVisibility::Hidden;

    // Stat text widgets for live updates
    TSharedPtr<class STextBlock> CharacterNameText;
    TSharedPtr<class STextBlock> CharacterLevelText;
    TSharedPtr<class STextBlock> StrengthText;
    TSharedPtr<class STextBlock> AgilityText;
    TSharedPtr<class STextBlock> StaminaText;
    TSharedPtr<class STextBlock> IntellectText;
    TSharedPtr<class STextBlock> SpiritText;
    TSharedPtr<class STextBlock> ArmorText;
    TSharedPtr<class STextBlock> HolyResText;
    TSharedPtr<class STextBlock> FireResText;
    TSharedPtr<class STextBlock> NatureResText;
    TSharedPtr<class STextBlock> FrostResText;
    TSharedPtr<class STextBlock> ShadowResText;
    TSharedPtr<class STextBlock> ArcaneResText;
    TSharedPtr<class STextBlock> AttackPowerText;
    TSharedPtr<class STextBlock> RangedAttackPowerText;
};
