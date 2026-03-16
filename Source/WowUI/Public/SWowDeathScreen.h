#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

DECLARE_DELEGATE(FOnReleaseSpirit);

/**
 * Death screen overlay widget shown when the player dies
 */
class WOWUI_API SWowDeathScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowDeathScreen)
    {}
        SLATE_EVENT(FOnReleaseSpirit, OnReleaseSpirit)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Set the corpse reclaim timer */
    void SetCorpseReclaimTimer(float TimeInSeconds);

    /** Update the timer display */
    void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
    /** Event when Release Spirit button is clicked */
    FOnReleaseSpirit OnReleaseSpirit;

    /** Handle Release Spirit button click */
    FReply HandleReleaseSpiritClicked();

    /** Timer state */
    float CorpseReclaimTime = 0.0f;
    double StartTime = 0.0;
    bool bTimerActive = false;

    /** Get current timer text */
    FText GetTimerText() const;
    EVisibility GetTimerVisibility() const;
};