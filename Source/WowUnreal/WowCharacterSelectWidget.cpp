#include "WowCharacterSelectWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"

static FString GetRaceName(uint8 Race)
{
    switch (Race)
    {
    case 1: return TEXT("Human"); case 2: return TEXT("Orc"); case 3: return TEXT("Dwarf");
    case 4: return TEXT("Night Elf"); case 5: return TEXT("Undead"); case 6: return TEXT("Tauren");
    case 7: return TEXT("Gnome"); case 8: return TEXT("Troll"); case 10: return TEXT("Blood Elf");
    case 11: return TEXT("Draenei"); default: return FString::Printf(TEXT("Race%d"), Race);
    }
}

static FString GetClassName(uint8 Class)
{
    switch (Class)
    {
    case 1: return TEXT("Warrior"); case 2: return TEXT("Paladin"); case 3: return TEXT("Hunter");
    case 4: return TEXT("Rogue"); case 5: return TEXT("Priest"); case 6: return TEXT("Death Knight");
    case 7: return TEXT("Shaman"); case 8: return TEXT("Mage"); case 9: return TEXT("Warlock");
    case 11: return TEXT("Druid"); default: return FString::Printf(TEXT("Class%d"), Class);
    }
}

void SWowCharacterSelectWidget::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.05f, 0.85f))
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().FillHeight(1.0f)

        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 20, 0, 20)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Character Select")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
            .ColorAndOpacity(FLinearColor(1.0f, 0.84f, 0.0f))
        ]

        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(40, 0)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SAssignNew(CharListBox, SVerticalBox)
            ]
        ]

        // Buttons row
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 15)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(5)
            [
                SNew(SButton)
                .OnClicked_Lambda([this]() -> FReply
                {
                    OnCreateCharacterRequest.ExecuteIfBound();
                    return FReply::Handled();
                })
                [
                    SNew(SBox).Padding(FMargin(15, 5))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Create New")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                    ]
                ]
            ]
        ]

        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
        [
            SAssignNew(StatusLabel, STextBlock)
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
        ]

        + SVerticalBox::Slot().FillHeight(1.0f)
        ]
    ];
}

void SWowCharacterSelectWidget::PopulateCharacters(const TArray<FWowCharacterInfo>& Characters)
{
    if (!CharListBox.IsValid()) return;
    CharListBox->ClearChildren();
    CachedCharacters = Characters;

    if (Characters.Num() == 0)
    {
        SetStatusText(TEXT("No characters found. Create a new character!"));
        return;
    }

    for (int32 i = 0; i < Characters.Num(); i++)
    {
        const FWowCharacterInfo& C = Characters[i];
        FString Label = FString::Printf(TEXT("%s  —  Level %d %s %s"),
            *C.Name, C.Level, *GetRaceName(C.Race), *GetClassName(C.Class));
        int64 CharGuid = C.Guid;

        CharListBox->AddSlot().AutoHeight().Padding(2)
        [
            SNew(SButton)
            .OnClicked_Lambda([this, CharGuid]() -> FReply
            {
                OnCharacterSelected.ExecuteIfBound(CharGuid);
                return FReply::Handled();
            })
            [
                SNew(SBox).Padding(FMargin(10, 5))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Label))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                ]
            ]
        ];
    }

    SetStatusText(FString::Printf(TEXT("%d character(s) — click to enter world"), Characters.Num()));
}

void SWowCharacterSelectWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel.IsValid()) StatusLabel->SetText(FText::FromString(Text));
}
