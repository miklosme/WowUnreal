#include "WowModelViewerWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Formats/Dbc/DbcStore.h"

struct FMVRaceEntry { FString Name; uint8 Id; };

static const FMVRaceEntry GMVRaces[] = {
    {TEXT("Human"), 1}, {TEXT("Orc"), 2}, {TEXT("Dwarf"), 3}, {TEXT("Night Elf"), 4},
    {TEXT("Undead"), 5}, {TEXT("Tauren"), 6}, {TEXT("Gnome"), 7}, {TEXT("Troll"), 8},
    {TEXT("Blood Elf"), 10}, {TEXT("Draenei"), 11}
};

static const int32 GMVRaceCount = UE_ARRAY_COUNT(GMVRaces);

void SWowModelViewerWidget::Construct(const FArguments& InArgs)
{
    UpdateCustomizationRanges();

    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.06f, 0.9f))
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .Padding(FMargin(10))
        [
            SNew(SBox)
            .WidthOverride(320)
            [
                SNew(SScrollBox)

                // Title
                + SScrollBox::Slot().Padding(0, 5, 0, 15)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Model Viewer")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
                    .ColorAndOpacity(FLinearColor(1.0f, 0.84f, 0.0f))
                ]

                // --- Character Section ---
                + SScrollBox::Slot().Padding(0, 5)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("-- Character --")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                    .ColorAndOpacity(FLinearColor(0.7f, 0.85f, 1.0f))
                ]

                // Race
                + SScrollBox::Slot().Padding(0, 3)
                [
                    MakeCycleRow(TEXT("Race"), RaceText,
                        FOnClicked::CreateLambda([this]() -> FReply {
                            RaceIndex = (RaceIndex + GMVRaceCount - 1) % GMVRaceCount;
                            if (RaceText.IsValid()) RaceText->SetText(FText::FromString(GMVRaces[RaceIndex].Name));
                            SkinColor = FaceVariation = HairStyle = HairColor = FacialHairStyle = 0;
                            UpdateCustomizationRanges();
                            NotifyChanged();
                            return FReply::Handled();
                        }),
                        FOnClicked::CreateLambda([this]() -> FReply {
                            RaceIndex = (RaceIndex + 1) % GMVRaceCount;
                            if (RaceText.IsValid()) RaceText->SetText(FText::FromString(GMVRaces[RaceIndex].Name));
                            SkinColor = FaceVariation = HairStyle = HairColor = FacialHairStyle = 0;
                            UpdateCustomizationRanges();
                            NotifyChanged();
                            return FReply::Handled();
                        }))
                ]

                // Gender
                + SScrollBox::Slot().Padding(0, 3)
                [
                    MakeCycleRow(TEXT("Gender"), GenderText,
                        FOnClicked::CreateLambda([this]() -> FReply {
                            GenderIndex = 1 - GenderIndex;
                            if (GenderText.IsValid()) GenderText->SetText(FText::FromString(GenderIndex == 0 ? TEXT("Male") : TEXT("Female")));
                            SkinColor = FaceVariation = HairStyle = HairColor = FacialHairStyle = 0;
                            UpdateCustomizationRanges();
                            NotifyChanged();
                            return FReply::Handled();
                        }),
                        FOnClicked::CreateLambda([this]() -> FReply {
                            GenderIndex = 1 - GenderIndex;
                            if (GenderText.IsValid()) GenderText->SetText(FText::FromString(GenderIndex == 0 ? TEXT("Male") : TEXT("Female")));
                            SkinColor = FaceVariation = HairStyle = HairColor = FacialHairStyle = 0;
                            UpdateCustomizationRanges();
                            NotifyChanged();
                            return FReply::Handled();
                        }))
                ]

                // Skin
                + SScrollBox::Slot().Padding(0, 3)
                [
                    MakeCycleRow(TEXT("Skin"), SkinText,
                        FOnClicked::CreateLambda([this]() -> FReply {
                            SkinColor = (SkinColor + MaxSkinColors - 1) % MaxSkinColors;
                            if (SkinText.IsValid()) SkinText->SetText(FText::AsNumber(SkinColor));
                            NotifyChanged();
                            return FReply::Handled();
                        }),
                        FOnClicked::CreateLambda([this]() -> FReply {
                            SkinColor = (SkinColor + 1) % MaxSkinColors;
                            if (SkinText.IsValid()) SkinText->SetText(FText::AsNumber(SkinColor));
                            NotifyChanged();
                            return FReply::Handled();
                        }))
                ]

                // Face
                + SScrollBox::Slot().Padding(0, 3)
                [
                    MakeCycleRow(TEXT("Face"), FaceText,
                        FOnClicked::CreateLambda([this]() -> FReply {
                            FaceVariation = (FaceVariation + MaxFaces - 1) % MaxFaces;
                            if (FaceText.IsValid()) FaceText->SetText(FText::AsNumber(FaceVariation));
                            NotifyChanged();
                            return FReply::Handled();
                        }),
                        FOnClicked::CreateLambda([this]() -> FReply {
                            FaceVariation = (FaceVariation + 1) % MaxFaces;
                            if (FaceText.IsValid()) FaceText->SetText(FText::AsNumber(FaceVariation));
                            NotifyChanged();
                            return FReply::Handled();
                        }))
                ]

                // Hair Style
                + SScrollBox::Slot().Padding(0, 3)
                [
                    MakeCycleRow(TEXT("Hair"), HairStyleText,
                        FOnClicked::CreateLambda([this]() -> FReply {
                            HairStyle = (HairStyle + MaxHairStyles - 1) % MaxHairStyles;
                            if (HairStyleText.IsValid()) HairStyleText->SetText(FText::AsNumber(HairStyle));
                            NotifyChanged();
                            return FReply::Handled();
                        }),
                        FOnClicked::CreateLambda([this]() -> FReply {
                            HairStyle = (HairStyle + 1) % MaxHairStyles;
                            if (HairStyleText.IsValid()) HairStyleText->SetText(FText::AsNumber(HairStyle));
                            NotifyChanged();
                            return FReply::Handled();
                        }))
                ]

                // Hair Color
                + SScrollBox::Slot().Padding(0, 3)
                [
                    MakeCycleRow(TEXT("Hair Color"), HairColorText,
                        FOnClicked::CreateLambda([this]() -> FReply {
                            HairColor = (HairColor + MaxHairColors - 1) % MaxHairColors;
                            if (HairColorText.IsValid()) HairColorText->SetText(FText::AsNumber(HairColor));
                            NotifyChanged();
                            return FReply::Handled();
                        }),
                        FOnClicked::CreateLambda([this]() -> FReply {
                            HairColor = (HairColor + 1) % MaxHairColors;
                            if (HairColorText.IsValid()) HairColorText->SetText(FText::AsNumber(HairColor));
                            NotifyChanged();
                            return FReply::Handled();
                        }))
                ]

                // Facial Hair
                + SScrollBox::Slot().Padding(0, 3)
                [
                    MakeCycleRow(TEXT("Facial Hair"), FacialHairText,
                        FOnClicked::CreateLambda([this]() -> FReply {
                            FacialHairStyle = (FacialHairStyle + MaxFacialHairStyles - 1) % MaxFacialHairStyles;
                            if (FacialHairText.IsValid()) FacialHairText->SetText(FText::AsNumber(FacialHairStyle));
                            NotifyChanged();
                            return FReply::Handled();
                        }),
                        FOnClicked::CreateLambda([this]() -> FReply {
                            FacialHairStyle = (FacialHairStyle + 1) % MaxFacialHairStyles;
                            if (FacialHairText.IsValid()) FacialHairText->SetText(FText::AsNumber(FacialHairStyle));
                            NotifyChanged();
                            return FReply::Handled();
                        }))
                ]

                // --- Equipment Section ---
                + SScrollBox::Slot().Padding(0, 10, 0, 5)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("-- Equipment --")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                    .ColorAndOpacity(FLinearColor(0.7f, 0.85f, 1.0f))
                ]

                + SScrollBox::Slot().Padding(0, 3)
                [
                    MakeTextField(TEXT("Weapon ID"), WeaponField)
                ]

                + SScrollBox::Slot().Padding(0, 5)
                [
                    SNew(SButton)
                    .OnClicked_Lambda([this]() -> FReply {
                        NotifyChanged();
                        return FReply::Handled();
                    })
                    [
                        SNew(SBox).Padding(FMargin(10, 3))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Apply Equipment")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                        ]
                    ]
                ]

                // --- Creature Section ---
                + SScrollBox::Slot().Padding(0, 10, 0, 5)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("-- Creature --")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                    .ColorAndOpacity(FLinearColor(0.7f, 0.85f, 1.0f))
                ]

                + SScrollBox::Slot().Padding(0, 3)
                [
                    MakeTextField(TEXT("Display ID"), CreatureField)
                ]

                + SScrollBox::Slot().Padding(0, 5)
                [
                    SNew(SButton)
                    .OnClicked_Lambda([this]() -> FReply {
                        OnCreatureSpawn.ExecuteIfBound(GetCreatureDisplayId());
                        return FReply::Handled();
                    })
                    [
                        SNew(SBox).Padding(FMargin(10, 3))
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Spawn Creature")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                        ]
                    ]
                ]

                // Status
                + SScrollBox::Slot().Padding(0, 15, 0, 5)
                [
                    SAssignNew(StatusLabel, STextBlock)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                    .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
                    .AutoWrapText(true)
                ]
            ]
        ]
    ];

    // Set initial text values
    if (RaceText.IsValid()) RaceText->SetText(FText::FromString(GMVRaces[RaceIndex].Name));
    if (GenderText.IsValid()) GenderText->SetText(FText::FromString(TEXT("Male")));
    if (SkinText.IsValid()) SkinText->SetText(FText::AsNumber(0));
    if (FaceText.IsValid()) FaceText->SetText(FText::AsNumber(0));
    if (HairStyleText.IsValid()) HairStyleText->SetText(FText::AsNumber(0));
    if (HairColorText.IsValid()) HairColorText->SetText(FText::AsNumber(0));
    if (FacialHairText.IsValid()) FacialHairText->SetText(FText::AsNumber(0));
}

TSharedRef<SHorizontalBox> SWowModelViewerWidget::MakeCycleRow(const FString& Label,
    TSharedPtr<STextBlock>& ValueText, FOnClicked OnPrev, FOnClicked OnNext)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
            SNew(SBox).MinDesiredWidth(90)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Label))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
                .ColorAndOpacity(FLinearColor::White)
            ]
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(2)
        [
            SNew(SButton).OnClicked(OnPrev)
            [ SNew(STextBlock).Text(FText::FromString(TEXT("<"))) ]
        ]
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SBox).MinDesiredWidth(100).HAlign(HAlign_Center)
            [
                SAssignNew(ValueText, STextBlock)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
                .ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
            ]
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(2)
        [
            SNew(SButton).OnClicked(OnNext)
            [ SNew(STextBlock).Text(FText::FromString(TEXT(">"))) ]
        ];
}

TSharedRef<SHorizontalBox> SWowModelViewerWidget::MakeTextField(const FString& Label,
    TSharedPtr<SEditableTextBox>& TextField)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
            SNew(SBox).MinDesiredWidth(90)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Label))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
                .ColorAndOpacity(FLinearColor::White)
            ]
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f)
        [
            SAssignNew(TextField, SEditableTextBox)
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
            .HintText(FText::FromString(TEXT("0")))
        ];
}

void SWowModelViewerWidget::UpdateCustomizationRanges()
{
    uint32 RaceId = GMVRaces[RaceIndex].Id;
    uint32 Gender = static_cast<uint32>(GenderIndex);

    MaxSkinColors = 1;
    MaxFaces = 1;
    MaxHairStyles = 1;
    MaxHairColors = 1;
    MaxFacialHairStyles = 1;

    if (!FDbcStore::Get().IsLoaded()) return;

    for (const auto& E : FDbcStore::Get().CharSections().GetAll())
    {
        if (E.RaceID != RaceId || E.SexID != Gender) continue;
        switch (E.Type)
        {
            case 0: // Skin
                MaxSkinColors = FMath::Max(MaxSkinColors, E.Color + 1);
                break;
            case 1: // Face
                MaxFaces = FMath::Max(MaxFaces, E.Variation + 1);
                break;
            case 2: // Facial Hair
                MaxFacialHairStyles = FMath::Max(MaxFacialHairStyles, E.Variation + 1);
                break;
            case 3: // Hair
                MaxHairStyles = FMath::Max(MaxHairStyles, E.Variation + 1);
                MaxHairColors = FMath::Max(MaxHairColors, E.Color + 1);
                break;
            default:
                break;
        }
    }
}

void SWowModelViewerWidget::NotifyChanged()
{
    OnCharacterChanged.ExecuteIfBound();
}

FWowCharacterBuilder::ERace SWowModelViewerWidget::GetSelectedRace() const
{
    return static_cast<FWowCharacterBuilder::ERace>(GMVRaces[RaceIndex].Id);
}

FWowCharacterBuilder::EGender SWowModelViewerWidget::GetSelectedGender() const
{
    return static_cast<FWowCharacterBuilder::EGender>(GenderIndex);
}

uint32 SWowModelViewerWidget::GetWeaponDisplayId() const
{
    if (!WeaponField.IsValid()) return 0;
    FString Text = WeaponField->GetText().ToString().TrimStartAndEnd();
    return Text.IsEmpty() ? 0 : static_cast<uint32>(FCString::Atoi(*Text));
}

uint32 SWowModelViewerWidget::GetCreatureDisplayId() const
{
    if (!CreatureField.IsValid()) return 0;
    FString Text = CreatureField->GetText().ToString().TrimStartAndEnd();
    return Text.IsEmpty() ? 0 : static_cast<uint32>(FCString::Atoi(*Text));
}

void SWowModelViewerWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel.IsValid()) StatusLabel->SetText(FText::FromString(Text));
}
