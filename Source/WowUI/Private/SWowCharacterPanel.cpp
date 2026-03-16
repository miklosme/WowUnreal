#include "SWowCharacterPanel.h"
#include "WowInventoryManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/UnrealMemory.h"

void SWowEquipmentSlot::Construct(const FArguments& InArgs, TSharedPtr<FWowInventoryManager> InInventoryManager)
{
    InventoryManager = InInventoryManager;
    SlotIndex = InArgs._SlotIndex;
    SlotName = InArgs._SlotName;

    ChildSlot
    [
        SAssignNew(SlotBorder, SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .BorderBackgroundColor(this, &SWowEquipmentSlot::GetSlotBorderColor)
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
                    .BorderBackgroundColor(this, &SWowEquipmentSlot::GetItemQualityColor)
                    .Padding(0)
                ]

                // Slot label
                + SOverlay::Slot()
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(this, &SWowEquipmentSlot::GetSlotText)
                    .ColorAndOpacity(FLinearColor::White)
                    .Font(FAppStyle::GetFontStyle("TinyFont"))
                    .Justification(ETextJustify::Center)
                ]
            ]
        ]
    ];

    UpdateSlotDisplay();
}

void SWowEquipmentSlot::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    bIsHovered = true;

    // TODO: Show tooltip with item info
}

void SWowEquipmentSlot::OnMouseLeave(const FPointerEvent& MouseEvent)
{
    bIsHovered = false;
}

void SWowEquipmentSlot::UpdateSlotDisplay()
{
    // Force refresh
    Invalidate(EInvalidateWidget::Layout);
}

FSlateColor SWowEquipmentSlot::GetSlotBorderColor() const
{
    if (bIsHovered)
        return FLinearColor::Yellow;

    return FLinearColor(1.0f, 0.84f, 0.0f); // Gold border
}

FSlateColor SWowEquipmentSlot::GetItemQualityColor() const
{
    if (!InventoryManager.IsValid())
        return FLinearColor::Gray;

    const FWowItemSlot* ItemSlot = InventoryManager->GetEquippedItem(SlotIndex);
    if (!ItemSlot || ItemSlot->IsEmpty())
        return FLinearColor(0.2f, 0.2f, 0.2f); // Dark background for empty slot

    return FWowInventoryManager::GetQualityColor(ItemSlot->Quality);
}

FText SWowEquipmentSlot::GetSlotText() const
{
    if (!InventoryManager.IsValid())
        return FText::FromString(SlotName);

    const FWowItemSlot* ItemSlot = InventoryManager->GetEquippedItem(SlotIndex);
    if (!ItemSlot || ItemSlot->IsEmpty())
        return FText::FromString(SlotName); // Show slot name when empty

    // Show item entry ID when equipped (placeholder)
    return FText::AsNumber(ItemSlot->ItemEntry);
}

void SWowCharacterPanel::Construct(const FArguments& InArgs, TSharedPtr<FWowInventoryManager> InInventoryManager)
{
    InventoryManager = InInventoryManager;

    // Equipment slot layout (simplified)
    TSharedPtr<SVerticalBox> MainLayout;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
        .BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.9f))
        .Padding(8.0f)
        [
            SAssignNew(MainLayout, SVerticalBox)

            // Title bar
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 5)
            [
                SNew(STextBlock)
                .Text(NSLOCTEXT("WowUI", "CharacterTitle", "Character"))
                .Font(FAppStyle::GetFontStyle("NormalFont"))
                .ColorAndOpacity(FLinearColor::White)
                .Justification(ETextJustify::Center)
            ]
        ]
    ];

    // Create equipment slots in a simplified layout
    struct FSlotInfo
    {
        uint8 SlotIndex;
        const TCHAR* SlotName;
    };

    const FSlotInfo SlotInfos[] = {
        { FWowInventoryManager::EQUIP_SLOT_HEAD, TEXT("Head") },
        { FWowInventoryManager::EQUIP_SLOT_SHOULDERS, TEXT("Shoulders") },
        { FWowInventoryManager::EQUIP_SLOT_CHEST, TEXT("Chest") },
        { FWowInventoryManager::EQUIP_SLOT_LEGS, TEXT("Legs") },
        { FWowInventoryManager::EQUIP_SLOT_FEET, TEXT("Feet") },
        { FWowInventoryManager::EQUIP_SLOT_HANDS, TEXT("Hands") },
        { FWowInventoryManager::EQUIP_SLOT_WRISTS, TEXT("Wrists") },
        { FWowInventoryManager::EQUIP_SLOT_WAIST, TEXT("Waist") },
        { FWowInventoryManager::EQUIP_SLOT_BACK, TEXT("Back") },
        { FWowInventoryManager::EQUIP_SLOT_MAINHAND, TEXT("Main Hand") },
        { FWowInventoryManager::EQUIP_SLOT_OFFHAND, TEXT("Off Hand") },
        { FWowInventoryManager::EQUIP_SLOT_RANGED, TEXT("Ranged") }
    };

    EquipmentSlots.Empty();
    EquipmentSlots.Reserve(UE_ARRAY_COUNT(SlotInfos));

    // Create a grid layout for equipment slots (3 columns)
    TSharedPtr<SGridPanel> EquipmentGrid;
    MainLayout->AddSlot()
    .AutoHeight()
    [
        SAssignNew(EquipmentGrid, SGridPanel)
    ];

    int32 SlotCount = UE_ARRAY_COUNT(SlotInfos);
    for (int32 i = 0; i < SlotCount; ++i)
    {
        const FSlotInfo& Info = SlotInfos[i];
        int32 Row = i / 3;
        int32 Col = i % 3;

        TSharedPtr<SWowEquipmentSlot> EquipmentSlot;
        EquipmentGrid->AddSlot(Col, Row)
        .Padding(2.0f)
        [
            SAssignNew(EquipmentSlot, SWowEquipmentSlot, InventoryManager)
            .SlotIndex(Info.SlotIndex)
            .SlotName(Info.SlotName)
        ];

        EquipmentSlots.Add(EquipmentSlot);
    }

    // Bind to inventory changes
    if (InventoryManager.IsValid())
    {
        InventoryManager->OnInventoryChanged.AddRaw(this, &SWowCharacterPanel::OnInventoryChanged);
    }

    SetPanelVisibility(EVisibility::Hidden);
}

void SWowCharacterPanel::SetPanelVisibility(EVisibility InVisibility)
{
    CurrentVisibility = InVisibility;
    SWidget::SetVisibility(InVisibility);
}

void SWowCharacterPanel::ToggleVisibility()
{
    if (CurrentVisibility == EVisibility::Hidden)
    {
        SetPanelVisibility(EVisibility::SelfHitTestInvisible);
    }
    else
    {
        SetPanelVisibility(EVisibility::Hidden);
    }
}

bool SWowCharacterPanel::IsPanelVisible() const
{
    return CurrentVisibility != EVisibility::Hidden;
}

void SWowCharacterPanel::OnInventoryChanged()
{
    // Update all equipment slots
    for (auto& EquipmentSlot : EquipmentSlots)
    {
        if (EquipmentSlot.IsValid())
        {
            EquipmentSlot->UpdateSlotDisplay();
        }
    }
}