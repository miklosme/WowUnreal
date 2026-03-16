#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Loading screen shown between login and world entry.
 * Displays progress as terrain loads around the spawn point.
 */
class SWowLoadingScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowLoadingScreen) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetProgress(float Fraction, const FString& StatusText);

private:
    TSharedPtr<STextBlock> TitleLabel;
    TSharedPtr<STextBlock> StatusLabel;
    TSharedPtr<STextBlock> ProgressLabel;
};
