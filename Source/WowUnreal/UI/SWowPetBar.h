#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "WowEntity.h"

class SHorizontalBox;
class UWowConnectionManager;

class WOWUNREAL_API SWowPetBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowPetBar) {}
        SLATE_ARGUMENT(UWowConnectionManager*, ConnectionManager)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void RefreshPetActions();

private:
    struct FPetActionSlotView
    {
        FWowPetActionSlot SlotData;
        FString Label;
        float CooldownRemaining = 0.0f;
    };

    TWeakObjectPtr<UWowConnectionManager> ConnectionManager;
    TArray<FPetActionSlotView> Slots;
    TSharedPtr<SHorizontalBox> SlotContainer;

    TSharedRef<SWidget> CreatePetActionSlotWidget(int32 SlotIndex);
    FReply OnPetActionSlotClicked(int32 SlotIndex, bool bRightClick);
    FString GetSpellName(uint32 SpellId) const;
    FText GetTooltipText(int32 SlotIndex) const;
};
