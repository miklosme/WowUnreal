#include "WowCharacterCreateWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"

struct FRaceEntry { FString Name; uint8 Id; };
struct FClassEntry { FString Name; uint8 Id; };

static const FRaceEntry GRaces[] = {
    {TEXT("Human"), 1}, {TEXT("Orc"), 2}, {TEXT("Dwarf"), 3}, {TEXT("Night Elf"), 4},
    {TEXT("Undead"), 5}, {TEXT("Tauren"), 6}, {TEXT("Gnome"), 7}, {TEXT("Troll"), 8},
    {TEXT("Blood Elf"), 10}, {TEXT("Draenei"), 11}
};

static const FClassEntry GClasses[] = {
    {TEXT("Warrior"), 1}, {TEXT("Paladin"), 2}, {TEXT("Hunter"), 3}, {TEXT("Rogue"), 4},
    {TEXT("Priest"), 5}, {TEXT("Death Knight"), 6}, {TEXT("Shaman"), 7}, {TEXT("Mage"), 8},
    {TEXT("Warlock"), 9}, {TEXT("Druid"), 11}
};

void SWowCharacterCreateWidget::Construct(const FArguments& InArgs)
{
    // Build race/class button rows (simple cycle buttons instead of combo boxes for Slate simplicity)
    auto MakeCycleRow = [](const FString& Label, TSharedRef<STextBlock>& ValueText) -> TSharedRef<SHorizontalBox>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Label))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).MinDesiredWidth(200)
                [
                    SAssignNew(ValueText, STextBlock)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                    .ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
                ]
            ];
    };

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
            .Text(FText::FromString(TEXT("Create Character")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
            .ColorAndOpacity(FLinearColor(1.0f, 0.84f, 0.0f))
        ]

        // Name
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Name:")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).MinDesiredWidth(250)
                [
                    SAssignNew(NameBox, SEditableTextBox)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                    .HintText(FText::FromString(TEXT("Character name")))
                ]
            ]
        ]

        // Race selector (prev/next buttons)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Race:")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2)
            [
                SNew(SButton)
                .OnClicked_Lambda([this]() -> FReply
                {
                    SelectedRaceIndex = (SelectedRaceIndex + UE_ARRAY_COUNT(GRaces) - 1) % UE_ARRAY_COUNT(GRaces);
                    if (RaceText.IsValid()) RaceText->SetText(FText::FromString(GRaces[SelectedRaceIndex].Name));
                    return FReply::Handled();
                })
                [ SNew(STextBlock).Text(FText::FromString(TEXT("<"))) ]
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).MinDesiredWidth(120).HAlign(HAlign_Center)
                [
                    SAssignNew(RaceText, STextBlock)
                    .Text(FText::FromString(GRaces[0].Name))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                    .ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2)
            [
                SNew(SButton)
                .OnClicked_Lambda([this]() -> FReply
                {
                    SelectedRaceIndex = (SelectedRaceIndex + 1) % UE_ARRAY_COUNT(GRaces);
                    if (RaceText.IsValid()) RaceText->SetText(FText::FromString(GRaces[SelectedRaceIndex].Name));
                    return FReply::Handled();
                })
                [ SNew(STextBlock).Text(FText::FromString(TEXT(">"))) ]
            ]
        ]

        // Class selector
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Class:")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2)
            [
                SNew(SButton)
                .OnClicked_Lambda([this]() -> FReply
                {
                    SelectedClassIndex = (SelectedClassIndex + UE_ARRAY_COUNT(GClasses) - 1) % UE_ARRAY_COUNT(GClasses);
                    if (ClassText.IsValid()) ClassText->SetText(FText::FromString(GClasses[SelectedClassIndex].Name));
                    return FReply::Handled();
                })
                [ SNew(STextBlock).Text(FText::FromString(TEXT("<"))) ]
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).MinDesiredWidth(120).HAlign(HAlign_Center)
                [
                    SAssignNew(ClassText, STextBlock)
                    .Text(FText::FromString(GClasses[0].Name))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                    .ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2)
            [
                SNew(SButton)
                .OnClicked_Lambda([this]() -> FReply
                {
                    SelectedClassIndex = (SelectedClassIndex + 1) % UE_ARRAY_COUNT(GClasses);
                    if (ClassText.IsValid()) ClassText->SetText(FText::FromString(GClasses[SelectedClassIndex].Name));
                    return FReply::Handled();
                })
                [ SNew(STextBlock).Text(FText::FromString(TEXT(">"))) ]
            ]
        ]

        // Gender selector
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Gender:")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2)
            [
                SNew(SButton)
                .OnClicked_Lambda([this]() -> FReply
                {
                    SelectedGender = 1 - SelectedGender;
                    if (GenderText.IsValid()) GenderText->SetText(FText::FromString(SelectedGender == 0 ? TEXT("Male") : TEXT("Female")));
                    return FReply::Handled();
                })
                [ SNew(STextBlock).Text(FText::FromString(TEXT("<>"))) ]
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).MinDesiredWidth(100).HAlign(HAlign_Center)
                [
                    SAssignNew(GenderText, STextBlock)
                    .Text(FText::FromString(TEXT("Male")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                    .ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
                ]
            ]
        ]

        // Buttons
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 20)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(5)
            [
                SNew(SButton)
                .OnClicked_Lambda([this]() -> FReply
                {
                    FString Name = NameBox.IsValid() ? NameBox->GetText().ToString().TrimStartAndEnd() : TEXT("");
                    if (Name.IsEmpty())
                    {
                        SetStatusText(TEXT("Please enter a character name"));
                        return FReply::Handled();
                    }
                    SetStatusText(TEXT("Creating character..."));
                    uint8 Race = GRaces[SelectedRaceIndex].Id;
                    uint8 Class = GClasses[SelectedClassIndex].Id;
                    OnCharacterCreated.ExecuteIfBound(Name, Race, Class, static_cast<uint8>(SelectedGender), 0, 0, 0, 0, 0);
                    return FReply::Handled();
                })
                [
                    SNew(SBox).Padding(FMargin(15, 5))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Create")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
                    ]
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(5)
            [
                SNew(SButton)
                .OnClicked_Lambda([this]() -> FReply
                {
                    OnBackToCharSelect.ExecuteIfBound();
                    return FReply::Handled();
                })
                [
                    SNew(SBox).Padding(FMargin(15, 5))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Back")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 18))
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

void SWowCharacterCreateWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel.IsValid()) StatusLabel->SetText(FText::FromString(Text));
}
