#include "WowLoadingScreen.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"

void SWowLoadingScreen::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 1.0f))
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot().FillHeight(1.0f)

            // Title
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 20)
            [
                SAssignNew(TitleLabel, STextBlock)
                .Text(FText::FromString(TEXT("Entering World...")))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
                .ColorAndOpacity(FLinearColor(1.0f, 0.84f, 0.0f))
            ]

            // Status text
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
            [
                SAssignNew(StatusLabel, STextBlock)
                .Text(FText::FromString(TEXT("Loading terrain...")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                .ColorAndOpacity(FLinearColor(0.8f, 0.75f, 0.5f))
            ]

            // Progress
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 10)
            [
                SAssignNew(ProgressLabel, STextBlock)
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
            ]

            + SVerticalBox::Slot().FillHeight(1.0f)
        ]
    ];
}

void SWowLoadingScreen::SetProgress(float Fraction, const FString& StatusText)
{
    if (StatusLabel.IsValid())
    {
        StatusLabel->SetText(FText::FromString(StatusText));
    }
    if (ProgressLabel.IsValid())
    {
        int32 Pct = FMath::Clamp(FMath::RoundToInt32(Fraction * 100.0f), 0, 100);
        ProgressLabel->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), Pct)));
    }
}
