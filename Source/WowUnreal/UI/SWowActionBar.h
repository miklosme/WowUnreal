#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"

class UWowConnectionManager;
struct FSpellDbcEntry;

DECLARE_DELEGATE_OneParam(FOnActionSlotClicked, int32 /*SlotIndex*/);

class WOWUNREAL_API SWowActionBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowActionBar) {}
        SLATE_ARGUMENT(UWowConnectionManager*, ConnectionManager)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Update action bar from packet data */
    void RefreshActionButtons();

    /** Update cooldown for a specific spell */
    void UpdateCooldown(uint32 SpellId, float Duration);

private:
    /** Connection manager for spell casting and action data */
    TWeakObjectPtr<UWowConnectionManager> ConnectionManager;

    /** Action slot data */
    struct FActionSlot
    {
        uint32 PackedAction = 0;
        uint32 ActionId = 0;
        uint8 ActionType = 0;
        FString SpellName;
        FString KeybindText;
        bool bIsEmpty = true;
        double CooldownExpiry = 0.0;
        float CooldownDuration = 0.0f;

        void UpdateFromPacked(uint32 InPackedAction);
        bool IsOnCooldown() const { return FPlatformTime::Seconds() < CooldownExpiry; }
    };

    TArray<FActionSlot> ActionSlots;
    TSharedPtr<SHorizontalBox> SlotContainer;

    /** Create a single action slot widget */
    TSharedRef<SWidget> CreateActionSlotWidget(int32 SlotIndex);

    /** Handle click on action slot */
    FReply OnActionSlotClicked(int32 SlotIndex);

    /** Get tooltip text for action slot */
    FText GetSlotTooltipText(int32 SlotIndex) const;

    /** Get spell name from DBC */
    FString GetSpellName(uint32 SpellId) const;

    /** Get keybind text for slot */
    FString GetKeybindText(int32 SlotIndex) const;
};