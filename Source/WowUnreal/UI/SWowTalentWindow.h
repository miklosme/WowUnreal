#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

class UWowConnectionManager;
class FWowPacketHandler;
struct FTalentDbcEntry;
struct FTalentTabDbcEntry;
struct FWowTalentInfo;

/**
 * Talent tree window widget - displays talent trees and allows learning talents
 */
class WOWUNREAL_API SWowTalentWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowTalentWindow)
    {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UWowConnectionManager* InConnectionManager);

    /** Toggle visibility of the talent window */
    void ToggleVisibility();

    /** Check if talent window is visible */
    bool IsVisible() const;

    /** Update the talent data when talents are changed */
    void RefreshTalentData();

private:
    /** Connection manager for sending talent learn packets */
    TWeakObjectPtr<UWowConnectionManager> ConnectionManager;

    /** Current visibility state */
    EVisibility CurrentVisibility = EVisibility::Hidden;

    /** Current selected talent tab (0-2) */
    int32 CurrentTalentTab = 0;

    /** Tab button widgets */
    TArray<TSharedPtr<SButton>> TabButtons;

    /** Talent grid container (4x11 grid) */
    TSharedPtr<class SUniformGridPanel> TalentGrid;

    /** Available talent points text */
    TSharedPtr<STextBlock> TalentPointsText;

    /** Get player class from local player entity */
    uint8 GetPlayerClass() const;

    /** Get talent tabs for the current player class */
    TArray<const FTalentTabDbcEntry*> GetPlayerTalentTabs() const;

    /** Create tab buttons at the top of the window */
    TSharedRef<SWidget> CreateTabButtons();

    /** Create the talent grid for the current tab */
    TSharedRef<SWidget> CreateTalentGrid();

    /** Create a single talent slot widget */
    TSharedRef<SWidget> CreateTalentSlot(int32 Row, int32 Col);

    /** Get talent for the current tab and position */
    const FTalentDbcEntry* GetTalentAt(int32 Row, int32 Col) const;

    /** Get current rank of a talent */
    uint8 GetTalentRank(uint32 TalentId) const;

    /** Check if a talent can be learned (requirements met) */
    bool CanLearnTalent(const FTalentDbcEntry* TalentEntry) const;

    /** Get available talent points for the current tab */
    int32 GetAvailableTalentPoints() const;

    /** Get points spent in a specific talent tab */
    int32 GetPointsSpentInTab(int32 TabIndex) const;

    /** Handle tab button clicked */
    FReply OnTabButtonClicked(int32 TabIndex);

    /** Handle talent slot clicked */
    FReply OnTalentClicked(uint32 TalentId);

    /** Handle close button clicked */
    FReply OnCloseButtonClicked();

    /** Get talent icon color based on state */
    FLinearColor GetTalentColor(const FTalentDbcEntry* TalentEntry, uint8 CurrentRank) const;

    /** Get talent tooltip text */
    FText GetTalentTooltip(const FTalentDbcEntry* TalentEntry) const;

    /** Get spell data for a talent rank */
    const struct FSpellDbcEntry* GetTalentSpellData(const FTalentDbcEntry* TalentEntry, uint8 Rank) const;

    /** Format talent name and rank text */
    FText GetTalentDisplayText(const FTalentDbcEntry* TalentEntry, uint8 CurrentRank) const;

    /** Handle talent data updated from server */
    void OnTalentsUpdated();
};