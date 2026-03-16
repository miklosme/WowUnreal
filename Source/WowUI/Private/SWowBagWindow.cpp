#include "SWowBagWindow.h"
#include "WowInventoryManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"

void SWowItemSlot::Construct(const FArguments& InArgs, TSharedPtr<FWowInventoryManager> InInventoryManager)
{
    InventoryManager = InInventoryManager;
    SlotIndex = InArgs._SlotIndex;

    ChildSlot
    [
        SAssignNew(SlotBorder, SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .BorderBackgroundColor(this, &SWowItemSlot::GetSlotBorderColor)
        .Padding(2.0f)
        [
            SNew(SBox)
            .WidthOverride(40.0f)
            .HeightOverride(40.0f)
            [
                SNew(SOverlay)

                // Item background (colored by quality)
                + SOverlay::Slot()
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(this, &SWowItemSlot::GetItemQualityColor)
                    .Padding(0)
                ]

                // Stack count text
                + SOverlay::Slot()
                .HAlign(HAlign_Right)
                .VAlign(VAlign_Bottom)
                [
                    SAssignNew(StackCountText, STextBlock)
                    .Text(this, &SWowItemSlot::GetStackCountText)
                    .ColorAndOpacity(FLinearColor::White)
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                ]
            ]
        ]
    ];

    UpdateSlotDisplay();
}

FReply SWowItemSlot::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        // Right click to auto-equip
        if (InventoryManager.IsValid())
        {
            const FWowItemSlot* ItemSlot = InventoryManager->GetBackpackItem(SlotIndex);
            if (ItemSlot && !ItemSlot->IsEmpty())
            {
                InventoryManager->AutoEquipItem(255, SlotIndex); // 255 = backpack
                return FReply::Handled();
            }
        }
    }

    return FReply::Unhandled();
}

void SWowItemSlot::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    bIsHovered = true;

    // TODO: Show tooltip with item info
    // For now, just update border color
}

void SWowItemSlot::OnMouseLeave(const FPointerEvent& MouseEvent)
{
    bIsHovered = false;
}

void SWowItemSlot::UpdateSlotDisplay()
{
    // Force refresh
    Invalidate(EInvalidateWidget::Layout);
}

FSlateColor SWowItemSlot::GetSlotBorderColor() const
{
    if (bIsHovered)
        return FLinearColor::Yellow;

    return FLinearColor(1.0f, 0.84f, 0.0f); // Gold border
}

FSlateColor SWowItemSlot::GetItemQualityColor() const
{
    if (!InventoryManager.IsValid())
        return FLinearColor::Gray;

    const FWowItemSlot* ItemSlot = InventoryManager->GetBackpackItem(SlotIndex);
    if (!ItemSlot || ItemSlot->IsEmpty())
        return FLinearColor(0.2f, 0.2f, 0.2f); // Dark background for empty slot

    return FWowInventoryManager::GetQualityColor(ItemSlot->Quality);
}

FText SWowItemSlot::GetStackCountText() const
{
    if (!InventoryManager.IsValid())
        return FText::GetEmpty();

    const FWowItemSlot* ItemSlot = InventoryManager->GetBackpackItem(SlotIndex);
    if (!ItemSlot || ItemSlot->IsEmpty() || ItemSlot->StackCount <= 1)
        return FText::GetEmpty();

    return FText::AsNumber(ItemSlot->StackCount);
}

void SWowBagWindow::Construct(const FArguments& InArgs, TSharedPtr<FWowInventoryManager> InInventoryManager)
{
    InventoryManager = InInventoryManager;

    // Create 4x4 grid of item slots
    SAssignNew(ItemGrid, SGridPanel);

    ItemSlots.Empty();
    ItemSlots.Reserve(16);

    for (int32 Row = 0; Row < 4; ++Row)
    {
        for (int32 Col = 0; Col < 4; ++Col)
        {
            uint8 SlotIndex = Row * 4 + Col;

            TSharedPtr<SWowItemSlot> ItemSlot;
            ItemGrid->AddSlot(Col, Row)
            [
                SAssignNew(ItemSlot, SWowItemSlot, InventoryManager)
                .SlotIndex(SlotIndex)
            ];

            ItemSlots.Add(ItemSlot);
        }
    }

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
        .BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.9f))
        .Padding(8.0f)
        [
            SNew(SVerticalBox)

            // Title bar
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 5)
            [
                SNew(STextBlock)
                .Text(NSLOCTEXT("WowUI", "BackpackTitle", "Backpack"))
                .Font(FAppStyle::GetFontStyle("NormalFont"))
                .ColorAndOpacity(FLinearColor::White)
                .Justification(ETextJustify::Center)
            ]

            // Item grid
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                ItemGrid.ToSharedRef()
            ]
        ]
    ];

    // Bind to inventory changes
    if (InventoryManager.IsValid())
    {
        InventoryManager->OnInventoryChanged.AddRaw(this, &SWowBagWindow::OnInventoryChanged);
    }

    SetBagVisibility(EVisibility::Hidden);
}

void SWowBagWindow::SetBagVisibility(EVisibility InVisibility)
{
    CurrentVisibility = InVisibility;
    SWidget::SetVisibility(InVisibility);
}

void SWowBagWindow::ToggleVisibility()
{
    if (CurrentVisibility == EVisibility::Hidden)
    {
        SetBagVisibility(EVisibility::SelfHitTestInvisible);
    }
    else
    {
        SetBagVisibility(EVisibility::Hidden);
    }
}

bool SWowBagWindow::IsBagVisible() const
{
    return CurrentVisibility != EVisibility::Hidden;
}

void SWowBagWindow::OnInventoryChanged()
{
    // Update all item slots
    for (auto& ItemSlot : ItemSlots)
    {
        if (ItemSlot.IsValid())
        {
            ItemSlot->UpdateSlotDisplay();
        }
    }
}