#include "SWowPetBar.h"

#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "Formats/Dbc/DbcStore.h"
#include "Formats/Dbc/SpellDbc.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowPetBar, Log, All);

void SWowPetBar::Construct(const FArguments& InArgs)
{
    ConnectionManager = InArgs._ConnectionManager;

    Slots.Init(FPetActionSlotView{}, WOW_PET_ACTION_SLOT_COUNT);
    SAssignNew(SlotContainer, SHorizontalBox);

    for (int32 SlotIndex = 0; SlotIndex < WOW_PET_ACTION_SLOT_COUNT; ++SlotIndex)
    {
        SlotContainer->AddSlot()
        .Padding(1.5f)
        [
            CreatePetActionSlotWidget(SlotIndex)
        ];
    }

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("Border"))
        .Padding(FMargin(4.0f))
        [
            SlotContainer.ToSharedRef()
        ]
    ];

    SetVisibility(EVisibility::Collapsed);
    RefreshPetActions();
}

TSharedRef<SWidget> SWowPetBar::CreatePetActionSlotWidget(int32 SlotIndex)
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
        .Padding(2.0f)
        .OnMouseButtonDown_Lambda([this, SlotIndex](const FGeometry&, const FPointerEvent& Event) -> FReply
        {
            if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
            {
                return OnPetActionSlotClicked(SlotIndex, false);
            }

            if (Event.GetEffectingButton() == EKeys::RightMouseButton)
            {
                return OnPetActionSlotClicked(SlotIndex, true);
            }

            return FReply::Unhandled();
        })
        .ToolTipText_Lambda([this, SlotIndex]() -> FText
        {
            return GetTooltipText(SlotIndex);
        })
        [
            SNew(SBox)
            .WidthOverride(34.0f)
            .HeightOverride(34.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SColorBlock)
                    .Color_Lambda([this, SlotIndex]() -> FLinearColor
                    {
                        const FPetActionSlotView& Slot = Slots[SlotIndex];
                        if (!Slot.SlotData.IsUsable())
                        {
                            return FLinearColor(0.08f, 0.08f, 0.08f, 0.95f);
                        }

                        if (Slot.SlotData.IsCommand())
                        {
                            return FLinearColor(0.45f, 0.18f, 0.12f, 0.95f);
                        }

                        if (Slot.SlotData.IsReaction())
                        {
                            return FLinearColor(0.18f, 0.32f, 0.16f, 0.95f);
                        }

                        return Slot.SlotData.IsAutocastEnabled()
                            ? FLinearColor(0.12f, 0.32f, 0.45f, 0.95f)
                            : FLinearColor(0.12f, 0.18f, 0.36f, 0.95f);
                    })
                ]
                + SOverlay::Slot()
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this, SlotIndex]() -> FText
                    {
                        const FString& Label = Slots[SlotIndex].Label;
                        return Label.IsEmpty() ? FText::GetEmpty() : FText::FromString(Label.Left(4));
                    })
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                    .ColorAndOpacity(FSlateColor(FLinearColor::White))
                ]
                + SOverlay::Slot()
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Top)
                .Padding(1.0f)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this, SlotIndex]() -> FText
                    {
                        return Slots[SlotIndex].SlotData.IsAutocastEnabled()
                            ? FText::FromString(TEXT("A"))
                            : FText::GetEmpty();
                    })
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 7))
                    .ColorAndOpacity(FSlateColor(FLinearColor::Yellow))
                ]
                + SOverlay::Slot()
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Fill)
                [
                    SNew(SColorBlock)
                    .Visibility_Lambda([this, SlotIndex]() -> EVisibility
                    {
                        return Slots[SlotIndex].CooldownRemaining > 0.0f ? EVisibility::Visible : EVisibility::Collapsed;
                    })
                    .Color_Lambda([this, SlotIndex]() -> FLinearColor
                    {
                        const float Remaining = Slots[SlotIndex].CooldownRemaining;
                        return Remaining > 0.0f
                            ? FLinearColor(0.0f, 0.0f, 0.0f, 0.45f)
                            : FLinearColor::Transparent;
                    })
                ]
            ]
        ];
}

void SWowPetBar::RefreshPetActions()
{
    if (!ConnectionManager.IsValid())
    {
        SetVisibility(EVisibility::Collapsed);
        return;
    }

    const FWowPetActionBarState& PetBar = ConnectionManager->PacketHandler.PetActionBar;
    SetVisibility(PetBar.HasActionBar() ? EVisibility::Visible : EVisibility::Collapsed);

    for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
    {
        Slots[SlotIndex] = FPetActionSlotView{};

        const FWowPetActionSlot* Slot = PetBar.GetSlot(SlotIndex);
        if (!Slot)
        {
            continue;
        }

        Slots[SlotIndex].SlotData = *Slot;
        Slots[SlotIndex].CooldownRemaining = PetBar.GetCooldownRemaining(SlotIndex);

        if (Slot->IsSpell() && Slot->ActionId != 0)
        {
            Slots[SlotIndex].Label = GetSpellName(Slot->ActionId);
        }
        else
        {
            Slots[SlotIndex].Label = Slot->GetDisplayName();
        }
    }

    UE_LOG(LogWowPetBar, Verbose, TEXT("Refreshed pet bar for pet %llu"), PetBar.PetGuid);
}

FReply SWowPetBar::OnPetActionSlotClicked(int32 SlotIndex, bool bRightClick)
{
    if (!ConnectionManager.IsValid())
    {
        return FReply::Unhandled();
    }

    const FWowPetActionSlot* Slot = ConnectionManager->PacketHandler.PetActionBar.GetSlot(SlotIndex);
    if (!Slot || !Slot->IsUsable())
    {
        return FReply::Handled();
    }

    if (bRightClick && Slot->IsAutocastCapable())
    {
        ConnectionManager->SendPetSpellAutocast(SlotIndex, !Slot->IsAutocastEnabled());
    }
    else
    {
        ConnectionManager->SendPetActionBarSlot(SlotIndex);
    }

    RefreshPetActions();
    return FReply::Handled();
}

FString SWowPetBar::GetSpellName(uint32 SpellId) const
{
    if (FDbcStore::Get().IsLoaded())
    {
        if (const FSpellDbcEntry* SpellEntry = FDbcStore::Get().Spells().GetById(SpellId))
        {
            return SpellEntry->SpellName;
        }
    }

    return FString::Printf(TEXT("Spell %u"), SpellId);
}

FText SWowPetBar::GetTooltipText(int32 SlotIndex) const
{
    if (!Slots.IsValidIndex(SlotIndex))
    {
        return FText::GetEmpty();
    }

    const FPetActionSlotView& Slot = Slots[SlotIndex];
    if (!Slot.SlotData.IsUsable())
    {
        return FText::FromString(FString::Printf(TEXT("Pet Slot %d"), SlotIndex + 1));
    }

    FString Tooltip = Slot.Label.IsEmpty() ? Slot.SlotData.GetDisplayName() : Slot.Label;
    Tooltip += FString::Printf(TEXT("\nAction: %u\nType: 0x%02X"), Slot.SlotData.ActionId, Slot.SlotData.ActionType);

    if (Slot.SlotData.IsAutocastCapable())
    {
        Tooltip += Slot.SlotData.IsAutocastEnabled()
            ? TEXT("\nRight-click: disable autocast")
            : TEXT("\nRight-click: enable autocast");
    }

    return FText::FromString(Tooltip);
}
