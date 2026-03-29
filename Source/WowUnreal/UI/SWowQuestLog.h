#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

struct FWowQuestLogEntry;
class FWowPacketHandler;

/**
 * Quest log window widget
 */
class WOWUNREAL_API SWowQuestLog : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowQuestLog)
    {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, FWowPacketHandler* InPacketHandler);

    /** Update the quest list from the packet handler */
    void UpdateQuestList();

    /** Toggle visibility of the quest log window */
    void ToggleVisibility();

    /** Check if quest log is visible */
    bool IsVisible() const;

private:
    /** Create quest entry widget */
    TSharedRef<SWidget> CreateQuestEntry(const FWowQuestLogEntry& QuestEntry) const;

    /** Create objectives widget for a quest */
    TSharedRef<SWidget> CreateObjectivesWidget(const FWowQuestLogEntry& QuestEntry) const;

    /** Get quest name with fallback to ID */
    FString GetQuestName(uint32 QuestId) const;

    /** Get quest state text */
    FText GetQuestStateText(uint32 State) const;

    /** Get quest state color */
    FSlateColor GetQuestStateColor(uint32 State) const;

    /** Handle close button clicked */
    FReply OnCloseButtonClicked();

    /** Handle abandon quest button clicked */
    FReply OnAbandonQuest(uint32 QuestId) const;

    mutable FWowPacketHandler* PacketHandler = nullptr;
    TSharedPtr<class SScrollBox> QuestListScrollBox;
    EVisibility CurrentVisibility = EVisibility::Hidden;
};