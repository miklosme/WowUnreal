#include "SWowCastBar.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Engine/Engine.h"

void SWowCastBar::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(300.0f)
        .HeightOverride(40.0f)
        .Visibility(this, &SWowCastBar::GetCastBarVisibility)
        [
            SAssignNew(CastBarBorder, SBorder)
            .BorderBackgroundColor(FLinearColor(1.0f, 0.84f, 0.0f, 0.8f)) // Gold border
            .BorderImage(FCoreStyle::Get().GetBrush("Border"))
            .Padding(FMargin(3.0f))
            [
                SNew(SOverlay)
                +SOverlay::Slot()
                [
                    SNew(SBorder)
                    .BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.9f)) // Dark background
                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .Padding(FMargin(2.0f))
                    [
                        SAssignNew(CastProgressBar, SProgressBar)
                        .Percent(this, &SWowCastBar::GetProgressPercent)
                        .FillColorAndOpacity(FLinearColor(1.0f, 0.84f, 0.0f, 0.7f)) // Gold fill
                        .BackgroundImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    ]
                ]
                +SOverlay::Slot()
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SAssignNew(SpellNameText, STextBlock)
                    .Text(this, &SWowCastBar::GetSpellNameText)
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                    .ColorAndOpacity(FLinearColor::White)
                    .Justification(ETextJustify::Center)
                ]
            ]
        ]
    ];
}

void SWowCastBar::StartCast(const FString& SpellName, float Duration)
{
    bIsCasting = true;
    CurrentSpellName = SpellName;
    CastDuration = Duration;
    CurrentProgress = 0.0f;
}

void SWowCastBar::UpdateProgress(float Progress)
{
    CurrentProgress = FMath::Clamp(Progress, 0.0f, 1.0f);
}

void SWowCastBar::StopCast()
{
    bIsCasting = false;
    CurrentSpellName.Empty();
    CastDuration = 0.0f;
    CurrentProgress = 0.0f;
}

TOptional<float> SWowCastBar::GetProgressPercent() const
{
    if (!bIsCasting)
    {
        return 0.0f;
    }

    return CurrentProgress;
}

FText SWowCastBar::GetSpellNameText() const
{
    return FText::FromString(CurrentSpellName);
}

EVisibility SWowCastBar::GetCastBarVisibility() const
{
    return bIsCasting ? EVisibility::Visible : EVisibility::Hidden;
}