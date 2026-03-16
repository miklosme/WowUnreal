#include "WowCharacterCreateWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
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

// WoW-style colors
namespace WowCCUI
{
    const FLinearColor Gold(1.0f, 0.84f, 0.0f);
    const FLinearColor GoldDim(0.7f, 0.6f, 0.1f);
    const FLinearColor Background(0.02f, 0.04f, 0.08f, 0.95f);
    const FLinearColor PanelBg(0.03f, 0.03f, 0.06f, 0.95f);
    const FLinearColor PanelBorder(0.4f, 0.35f, 0.1f, 0.6f);
    const FLinearColor ButtonBg(0.15f, 0.12f, 0.02f, 1.0f);
    const FLinearColor ButtonBorder(0.35f, 0.28f, 0.05f, 1.0f);
    const FLinearColor ArrowBg(0.1f, 0.08f, 0.02f, 1.0f);
    const FLinearColor FieldBg(0.0f, 0.0f, 0.0f, 0.6f);
}

void SWowCharacterCreateWidget::Construct(const FArguments& InArgs)
{
    auto MakeCycleRow = [&](const FString& Label, TSharedPtr<STextBlock>& ValueText,
        FOnClicked OnPrev, FOnClicked OnNext) -> TSharedRef<SWidget>
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
            [
                SNew(SBox).MinDesiredWidth(100)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Label))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
                    .ColorAndOpacity(WowCCUI::GoldDim)
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2)
            [
                SNew(SButton)
                .ButtonColorAndOpacity(WowCCUI::ArrowBg)
                .OnClicked(OnPrev)
                [
                    SNew(SBox).Padding(FMargin(8, 2))
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT("<")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                        .ColorAndOpacity(WowCCUI::Gold)
                    ]
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).MinDesiredWidth(130).HAlign(HAlign_Center)
                [
                    SAssignNew(ValueText, STextBlock)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                    .ColorAndOpacity(FLinearColor(0.95f, 0.9f, 0.7f))
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().Padding(2)
            [
                SNew(SButton)
                .ButtonColorAndOpacity(WowCCUI::ArrowBg)
                .OnClicked(OnNext)
                [
                    SNew(SBox).Padding(FMargin(8, 2))
                    [
                        SNew(STextBlock).Text(FText::FromString(TEXT(">")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                        .ColorAndOpacity(WowCCUI::Gold)
                    ]
                ]
            ];
    };

    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(WowCCUI::Background)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        [
            SNew(SHorizontalBox)

            // ═══ Left panel: Character options ═══
            + SHorizontalBox::Slot().AutoWidth().Padding(30, 30, 15, 30)
            [
                SNew(SBorder)
                .BorderBackgroundColor(WowCCUI::PanelBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                .Padding(FMargin(1))
                [
                    SNew(SBorder)
                    .BorderBackgroundColor(WowCCUI::PanelBg)
                    .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                    .Padding(FMargin(25, 20))
                    [
                        SNew(SVerticalBox)

                        // Title
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 18)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Create Character")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 22))
                            .ColorAndOpacity(WowCCUI::Gold)
                        ]

                        // Name
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Name")))
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                                .ColorAndOpacity(WowCCUI::GoldDim)
                            ]
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                SNew(SBorder)
                                .BorderBackgroundColor(WowCCUI::FieldBg)
                                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                .Padding(FMargin(2))
                                [
                                    SNew(SBox).MinDesiredWidth(260)
                                    [
                                        SAssignNew(NameBox, SEditableTextBox)
                                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                                        .HintText(FText::FromString(TEXT("Character name")))
                                        .BackgroundColor(FLinearColor(0.05f, 0.05f, 0.1f, 1.0f))
                                        .ForegroundColor(FSlateColor(FLinearColor::White))
                                    ]
                                ]
                            ]
                        ]

                        // Race
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 3)
                        [
                            MakeCycleRow(TEXT("Race"), RaceText,
                                FOnClicked::CreateLambda([this]() -> FReply {
                                    SelectedRaceIndex = (SelectedRaceIndex + UE_ARRAY_COUNT(GRaces) - 1) % UE_ARRAY_COUNT(GRaces);
                                    if (RaceText.IsValid()) RaceText->SetText(FText::FromString(GRaces[SelectedRaceIndex].Name));
                                    return FReply::Handled();
                                }),
                                FOnClicked::CreateLambda([this]() -> FReply {
                                    SelectedRaceIndex = (SelectedRaceIndex + 1) % UE_ARRAY_COUNT(GRaces);
                                    if (RaceText.IsValid()) RaceText->SetText(FText::FromString(GRaces[SelectedRaceIndex].Name));
                                    return FReply::Handled();
                                }))
                        ]

                        // Class
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
                        [
                            MakeCycleRow(TEXT("Class"), ClassText,
                                FOnClicked::CreateLambda([this]() -> FReply {
                                    SelectedClassIndex = (SelectedClassIndex + UE_ARRAY_COUNT(GClasses) - 1) % UE_ARRAY_COUNT(GClasses);
                                    if (ClassText.IsValid()) ClassText->SetText(FText::FromString(GClasses[SelectedClassIndex].Name));
                                    return FReply::Handled();
                                }),
                                FOnClicked::CreateLambda([this]() -> FReply {
                                    SelectedClassIndex = (SelectedClassIndex + 1) % UE_ARRAY_COUNT(GClasses);
                                    if (ClassText.IsValid()) ClassText->SetText(FText::FromString(GClasses[SelectedClassIndex].Name));
                                    return FReply::Handled();
                                }))
                        ]

                        // Gender
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
                        [
                            MakeCycleRow(TEXT("Gender"), GenderText,
                                FOnClicked::CreateLambda([this]() -> FReply {
                                    SelectedGender = 1 - SelectedGender;
                                    if (GenderText.IsValid()) GenderText->SetText(FText::FromString(SelectedGender == 0 ? TEXT("Male") : TEXT("Female")));
                                    return FReply::Handled();
                                }),
                                FOnClicked::CreateLambda([this]() -> FReply {
                                    SelectedGender = 1 - SelectedGender;
                                    if (GenderText.IsValid()) GenderText->SetText(FText::FromString(SelectedGender == 0 ? TEXT("Male") : TEXT("Female")));
                                    return FReply::Handled();
                                }))
                        ]

                        // Buttons
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 0)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                            [
                                SNew(SBorder)
                                .BorderBackgroundColor(WowCCUI::ButtonBorder)
                                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                .Padding(FMargin(1))
                                [
                                    SNew(SButton)
                                    .ButtonColorAndOpacity(WowCCUI::ButtonBg)
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
                                        SNew(SBox).Padding(FMargin(20, 5))
                                        [
                                            SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("Create")))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
                                            .ColorAndOpacity(WowCCUI::Gold)
                                        ]
                                    ]
                                ]
                            ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SBorder)
                                .BorderBackgroundColor(WowCCUI::ButtonBorder)
                                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                .Padding(FMargin(1))
                                [
                                    SNew(SButton)
                                    .ButtonColorAndOpacity(WowCCUI::ButtonBg)
                                    .OnClicked_Lambda([this]() -> FReply
                                    {
                                        OnBackToCharSelect.ExecuteIfBound();
                                        return FReply::Handled();
                                    })
                                    [
                                        SNew(SBox).Padding(FMargin(20, 5))
                                        [
                                            SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("Back")))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
                                            .ColorAndOpacity(FLinearColor(0.7f, 0.65f, 0.4f))
                                        ]
                                    ]
                                ]
                            ]
                        ]

                        // Status
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
                        [
                            SAssignNew(StatusLabel, STextBlock)
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                            .ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.3f))
                        ]
                    ]
                ]
            ]

            // ═══ Right side: 3D character preview area (placeholder) ═══
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(15, 30, 30, 30)
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f))
                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().FillHeight(1.0f).HAlign(HAlign_Center).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("Character Preview")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                        .ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.4f))
                    ]
                ]
            ]
        ]
    ];

    // Set initial values
    if (RaceText.IsValid()) RaceText->SetText(FText::FromString(GRaces[0].Name));
    if (ClassText.IsValid()) ClassText->SetText(FText::FromString(GClasses[0].Name));
    if (GenderText.IsValid()) GenderText->SetText(FText::FromString(TEXT("Male")));
}

void SWowCharacterCreateWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel.IsValid()) StatusLabel->SetText(FText::FromString(Text));
}
