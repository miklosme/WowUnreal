#pragma once
#include "CoreMinimal.h"
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

private:
    TSharedPtr<class SImage> BackgroundImage;
    TSharedPtr<class STextBlock> ProgressText;
};