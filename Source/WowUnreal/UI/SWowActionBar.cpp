#include "SWowActionBar.h"
#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "Formats/Dbc/DbcStore.h"
#include "Formats/Dbc/SpellDbc.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowActionBar, Log, All);

void SWowActionBar::FActionSlot::UpdateFromPacked(uint32 InPackedAction)
{
    PackedAction = InPackedAction;
    ActionId = PackedAction & 0x00FFFFFF; // Lower 24 bits
    ActionType = (PackedAction >> 24) & 0xFF; // Upper 8 bits
    bIsEmpty = (PackedAction == 0);
}

void SWowActionBar::Construct(const FArguments& InArgs)
{
    ConnectionManager = InArgs._ConnectionManager;

    // Initialize 12 action slots for the main action bar
    ActionSlots.SetNum(12);
    for (int32 i = 0; i < 12; ++i)
    {
        ActionSlots[i].KeybindText = GetKeybindText(i);
    }

    // Create the horizontal container for action slots
    SAssignNew(SlotContainer, SHorizontalBox);

    for (int32 i = 0; i < 12; ++i)
    {
        SlotContainer->AddSlot()
        .Padding(2.0f)
        [
            CreateActionSlotWidget(i)
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

    RefreshActionButtons();
}

TSharedRef<SWidget> SWowActionBar::CreateActionSlotWidget(int32 SlotIndex)
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
        .Padding(2.0f)
        .OnMouseButtonDown_Lambda([this, SlotIndex](const FGeometry& Geometry, const FPointerEvent& Event) -> FReply
        {
            if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
            {
                return OnActionSlotClicked(SlotIndex);
            }
            return FReply::Unhandled();
        })
        .ToolTipText_Lambda([this, SlotIndex]() -> FText
        {
            return GetSlotTooltipText(SlotIndex);
        })
        [
            SNew(SBox)
            .WidthOverride(40.0f)
            .HeightOverride(40.0f)
            [
                SNew(SOverlay)

                // Background/Icon
                + SOverlay::Slot()
                .HAlign(HAlign_Fill)
                .VAlign(VAlign_Fill)
                [
                    SNew(SImage)
                    .Image_Lambda([this, SlotIndex]() -> const FSlateBrush*
                    {
                        if (!ActionSlots[SlotIndex].bIsEmpty)
                        {
                            // Placeholder colored square for spell icon
                            static FSlateBrush SpellBrush;
                            SpellBrush.TintColor = FSlateColor(FLinearColor::Blue);
                            SpellBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
                            return &SpellBrush;
                        }
                        return FCoreStyle::Get().GetBrush("Checkerboard");
                    })
                ]

                // Slot number
                + SOverlay::Slot()
                .HAlign(HAlign_Left)
                .VAlign(VAlign_Bottom)
                .Padding(2.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(ActionSlots[SlotIndex].KeybindText))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]

                // Spell name (if available)
                + SOverlay::Slot()
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this, SlotIndex]() -> FText
                    {
                        if (!ActionSlots[SlotIndex].bIsEmpty && !ActionSlots[SlotIndex].SpellName.IsEmpty())
                        {
                            return FText::FromString(ActionSlots[SlotIndex].SpellName.Left(3)); // Abbreviated
                        }
                        return FText::GetEmpty();
                    })
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 6))
                    .ColorAndOpacity(FSlateColor::UseForeground())
                ]
            ]
        ];
}

FReply SWowActionBar::OnActionSlotClicked(int32 SlotIndex)
{
    if (!ActionSlots.IsValidIndex(SlotIndex) || ActionSlots[SlotIndex].bIsEmpty)
    {
        return FReply::Handled();
    }

    const FActionSlot& Slot = ActionSlots[SlotIndex];

    // Only handle spell actions for now
    if (Slot.ActionType == 0 && Slot.ActionId > 0)
    {
        if (ConnectionManager.IsValid())
        {
            ConnectionManager->SendCastSpell(Slot.ActionId);
            UE_LOG(LogWowActionBar, Log, TEXT("Casting spell %u from slot %d"), Slot.ActionId, SlotIndex);
        }
    }

    return FReply::Handled();
}

FText SWowActionBar::GetSlotTooltipText(int32 SlotIndex) const
{
    if (!ActionSlots.IsValidIndex(SlotIndex) || ActionSlots[SlotIndex].bIsEmpty)
    {
        return FText::FromString(FString::Printf(TEXT("Empty Slot %d\nKeybind: %s"),
            SlotIndex + 1, *ActionSlots[SlotIndex].KeybindText));
    }

    const FActionSlot& Slot = ActionSlots[SlotIndex];
    FString TooltipText = FString::Printf(TEXT("Slot %d\nAction ID: %u\nType: %u\nKeybind: %s"),
        SlotIndex + 1, Slot.ActionId, Slot.ActionType, *Slot.KeybindText);

    if (!Slot.SpellName.IsEmpty())
    {
        TooltipText = FString::Printf(TEXT("%s\nSpell: %s"), *Slot.SpellName, *TooltipText);
    }

    return FText::FromString(TooltipText);
}

void SWowActionBar::RefreshActionButtons()
{
    if (!ConnectionManager.IsValid())
    {
        return;
    }

    const TArray<uint32>& ActionButtons = ConnectionManager->PacketHandler.ActionButtons;

    // Update first 12 slots (main action bar)
    for (int32 i = 0; i < 12 && i < ActionButtons.Num(); ++i)
    {
        ActionSlots[i].UpdateFromPacked(ActionButtons[i]);

        // Look up spell name if it's a spell action
        if (ActionSlots[i].ActionType == 0 && ActionSlots[i].ActionId > 0)
        {
            ActionSlots[i].SpellName = GetSpellName(ActionSlots[i].ActionId);
        }
        else
        {
            ActionSlots[i].SpellName.Empty();
        }
    }

    UE_LOG(LogWowActionBar, Log, TEXT("Refreshed action bar with %d buttons"), ActionButtons.Num());
}

FString SWowActionBar::GetSpellName(uint32 SpellId) const
{
    if (FDbcStore::Get().IsLoaded())
    {
        const FSpellDbcEntry* SpellEntry = FDbcStore::Get().Spells().GetById(SpellId);
        if (SpellEntry)
        {
            return SpellEntry->SpellName;
        }
    }
    return FString::Printf(TEXT("Spell %u"), SpellId);
}

FString SWowActionBar::GetKeybindText(int32 SlotIndex) const
{
    switch (SlotIndex)
    {
    case 0: return TEXT("1");
    case 1: return TEXT("2");
    case 2: return TEXT("3");
    case 3: return TEXT("4");
    case 4: return TEXT("5");
    case 5: return TEXT("6");
    case 6: return TEXT("7");
    case 7: return TEXT("8");
    case 8: return TEXT("9");
    case 9: return TEXT("0");
    case 10: return TEXT("-");
    case 11: return TEXT("=");
    default: return TEXT("");
    }
}