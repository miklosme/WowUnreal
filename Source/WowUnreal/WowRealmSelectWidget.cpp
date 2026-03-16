#include "WowRealmSelectWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"

// Shared WoW-style colors
namespace WowUI
{
    const FLinearColor Gold(1.0f, 0.84f, 0.0f);
    const FLinearColor GoldDim(0.7f, 0.6f, 0.1f);
    const FLinearColor Background(0.02f, 0.04f, 0.08f, 0.95f);
    const FLinearColor PanelBg(0.03f, 0.03f, 0.06f, 0.95f);
    const FLinearColor PanelBorder(0.4f, 0.35f, 0.1f, 0.6f);
    const FLinearColor ButtonBg(0.08f, 0.07f, 0.03f, 1.0f);
    const FLinearColor ButtonBorder(0.35f, 0.28f, 0.05f, 1.0f);
    const FLinearColor ListItem(0.06f, 0.06f, 0.1f, 0.8f);
    const FLinearColor ListItemHover(0.1f, 0.09f, 0.04f, 0.9f);
}

void SWowRealmSelectWidget::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(WowUI::Background)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(1.0f)

            // Title
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 20, 0, 5)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("World of Warcraft")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                .ColorAndOpacity(WowUI::GoldDim)
            ]

            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 25)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Select Realm")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
                .ColorAndOpacity(WowUI::Gold)
            ]

            // Realm list panel
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(40, 0)
            [
                SNew(SBorder)
                .BorderBackgroundColor(WowUI::PanelBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                .Padding(FMargin(1))
                [
                    SNew(SBorder)
                    .BorderBackgroundColor(WowUI::PanelBg)
                    .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                    .Padding(FMargin(15, 12))
                    [
                        SNew(SBox).MinDesiredWidth(400).MaxDesiredHeight(300)
                        [
                            SNew(SScrollBox)
                            + SScrollBox::Slot()
                            [
                                SAssignNew(RealmListBox, SVerticalBox)
                            ]
                        ]
                    ]
                ]
            ]

            // Status
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 12)
            [
                SAssignNew(StatusLabel, STextBlock)
                .Text(FText::FromString(TEXT("Loading realms...")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                .ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.3f))
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

        // Realm type string
        FString TypeStr;
        switch (R.Type)
        {
        case 0: TypeStr = TEXT("Normal"); break;
        case 1: TypeStr = TEXT("PvP"); break;
        case 6: TypeStr = TEXT("RP"); break;
        case 8: TypeStr = TEXT("RP-PvP"); break;
        default: TypeStr = FString::Printf(TEXT("Type %d"), R.Type); break;
        }

        FString Label = FString::Printf(TEXT("%s  —  %s  (%d chars)"), *R.Name, *TypeStr, R.CharacterCount);
        int32 RealmIndex = i;

        RealmListBox->AddSlot().AutoHeight().Padding(0, 2)
        [
            SNew(SBorder)
            .BorderBackgroundColor(WowUI::ButtonBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
            .Padding(FMargin(1))
            [
                SNew(SButton)
                .ButtonColorAndOpacity(WowUI::ListItem)
                .OnClicked_Lambda([this, RealmIndex]() -> FReply
                {
                    OnRealmSelected.ExecuteIfBound(RealmIndex);
                    return FReply::Handled();
                })
                [
                    SNew(SBox).Padding(FMargin(12, 6))
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Label))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                        .ColorAndOpacity(FLinearColor(0.9f, 0.85f, 0.65f))
                    ]
                ]
            ]
        ];
    }

    SetStatusText(FString::Printf(TEXT("%d realm(s) available — select one to continue"), Realms.Num()));
}

void SWowRealmSelectWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel.IsValid()) StatusLabel->SetText(FText::FromString(Text));
}
