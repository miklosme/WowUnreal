#pragma once
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class UTexture2D;

/**
 * Loading screen widget that displays WoW loading screen images with progress text overlay.
 */
class SWowLoadingScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowLoadingScreen) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Set the background loading screen image */
    void SetBackgroundImage(UTexture2D* Texture);

    /** Update the progress text */
    void SetProgressText(const FString& Text);

    /** Update progress with fraction and status (convenience) */
    void SetProgress(float Fraction, const FString& StatusText);

private:
    TSharedPtr<class SImage> BackgroundImage;
    TSharedPtr<class STextBlock> ProgressText;
    TStrongObjectPtr<UTexture2D> BackgroundTexture;
    FSlateBrush BackgroundBrush;
};
