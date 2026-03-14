#include "WowRealmSelectWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"

void SWowRealmSelectWidget::Construct(const FArguments& InArgs)
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
            .Text(FText::FromString(TEXT("Select Realm")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
            .ColorAndOpacity(FLinearColor(1.0f, 0.84f, 0.0f))
        ]

        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(40, 0)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SAssignNew(RealmListBox, SVerticalBox)
            ]
        ]

        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 10)
        [
            SAssignNew(StatusLabel, STextBlock)
            .Text(FText::FromString(TEXT("Loading realms...")))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
        ]

        + SVerticalBox::Slot().FillHeight(1.0f)
        ]
    ];
}

void SWowRealmSelectWidget::PopulateRealms(const TArray<FWowRealmInfo>& Realms)
{
    if (!RealmListBox.IsValid()) return;
    RealmListBox->ClearChildren();

    for (int32 i = 0; i < Realms.Num(); i++)
    {
        const FWowRealmInfo& R = Realms[i];
        FString Label = FString::Printf(TEXT("%s  (Type: %d, Characters: %d)"), *R.Name, R.Type, R.CharacterCount);
        int32 RealmIndex = i;

        RealmListBox->AddSlot().AutoHeight().Padding(2)
        [
            SNew(SButton)
            .OnClicked_Lambda([this, RealmIndex]() -> FReply
            {
                OnRealmSelected.ExecuteIfBound(RealmIndex);
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

    SetStatusText(FString::Printf(TEXT("%d realm(s) available"), Realms.Num()));
}

void SWowRealmSelectWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel.IsValid()) StatusLabel->SetText(FText::FromString(Text));
}
