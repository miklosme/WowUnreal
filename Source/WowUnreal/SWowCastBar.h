#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"

class WOWUNREAL_API SWowCastBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowCastBar)
        {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Start casting a spell */
    void StartCast(const FString& SpellName, float Duration);

    /** Update cast progress (0.0 to 1.0) */
    void UpdateProgress(float Progress);

    /** Complete or interrupt the cast */
    void StopCast();

    /** Check if currently casting */
    bool IsCasting() const { return bIsCasting; }

private:
    bool bIsCasting = false;
    FString CurrentSpellName;
    float CastDuration = 0.0f;
    float CurrentProgress = 0.0f;

    TSharedPtr<STextBlock> SpellNameText;
    TSharedPtr<SProgressBar> CastProgressBar;
    TSharedPtr<SBorder> CastBarBorder;

    /** Get the current progress as a fraction */
    TOptional<float> GetProgressPercent() const;

    /** Get the spell name text */
    FText GetSpellNameText() const;

    /** Get visibility for the cast bar */
    EVisibility GetCastBarVisibility() const;
};