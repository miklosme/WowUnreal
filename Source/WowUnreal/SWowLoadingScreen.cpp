#include "SWowLoadingScreen.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

void SWowLoadingScreen::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SOverlay)
        + SOverlay::Slot()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Fill)
        [
            // Background image
            SAssignNew(BackgroundImage, SImage)
            .Image(FCoreStyle::Get().GetBrush("Black")) // Default black background
        ]
        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Bottom)
        .Padding(FMargin(0.0f, 0.0f, 0.0f, 50.0f))
        [
            // Progress text overlay
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))
            .BorderImage(FCoreStyle::Get().GetBrush("NoBorder"))
            .Padding(FMargin(20.0f, 10.0f))
            [
                SAssignNew(ProgressText, STextBlock)
                .Text(FText::FromString(TEXT("Loading...")))
                .ColorAndOpacity(FLinearColor::White)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                .Justification(ETextJustify::Center)
            ]
        ]
    ];
}

void SWowLoadingScreen::SetBackgroundImage(UTexture2D* Texture)
{
    if (!BackgroundImage.IsValid())
    {
        return;
    }

    if (Texture)
    {
        BackgroundTexture.Reset(Texture);
        BackgroundBrush = FSlateBrush();
        BackgroundBrush.SetResourceObject(BackgroundTexture.Get());
        BackgroundBrush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
        BackgroundBrush.DrawAs = ESlateBrushDrawType::Image;
        BackgroundImage->SetImage(&BackgroundBrush);
    }
    else
    {
        BackgroundTexture.Reset();
        BackgroundBrush = FSlateBrush();
        BackgroundImage->SetImage(FCoreStyle::Get().GetBrush("Black"));
    }
}

void SWowLoadingScreen::SetProgressText(const FString& Text)
{
    if (ProgressText.IsValid())
    {
        ProgressText->SetText(FText::FromString(Text));
    }
}

void SWowLoadingScreen::SetProgress(float Fraction, const FString& StatusText)
{
    int32 Pct = FMath::Clamp(FMath::RoundToInt32(Fraction * 100.0f), 0, 100);
    SetProgressText(FString::Printf(TEXT("%s (%d%%)"), *StatusText, Pct));
}
