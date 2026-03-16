#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "WowEntity.h"

class SScrollBox;
class SButton;
class SHorizontalBox;

DECLARE_DELEGATE_TwoParams(FOnQuestDialogAccept, uint64 /*QuestGiverGuid*/, uint32 /*QuestId*/);
DECLARE_DELEGATE_ThreeParams(FOnQuestDialogComplete, uint64 /*QuestGiverGuid*/, uint32 /*QuestId*/, int32 /*RewardChoice*/);
DECLARE_DELEGATE(FOnQuestDialogDecline);

/**
 * WoW-style quest dialog widget
 * Shows quest details with title, description, objectives, and rewards
 * Has Accept/Decline buttons or Complete Quest button depending on mode
 */
class WOWUI_API SWowQuestDialog : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowQuestDialog) {}
        SLATE_EVENT(FOnQuestDialogAccept, OnQuestAccept)
        SLATE_EVENT(FOnQuestDialogComplete, OnQuestComplete)
        SLATE_EVENT(FOnQuestDialogDecline, OnQuestDecline)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Show quest details dialog (for new quest) */
    void ShowQuestDetails(const FWowQuestDetails& QuestDetails);

    /** Show quest reward dialog (for quest completion) */
    void ShowQuestReward(const FWowQuestDetails& QuestDetails);

    /** Hide the quest dialog */
    void CloseDialog();

private:
    FOnQuestDialogAccept OnQuestAccept;
    FOnQuestDialogComplete OnQuestComplete;
    FOnQuestDialogDecline OnQuestDecline;

    TSharedPtr<SScrollBox> ContentArea;
    TSharedPtr<SHorizontalBox> ButtonArea;
    TSharedPtr<SHorizontalBox> RewardArea;

    FWowQuestDetails CurrentQuest;
    bool bIsRewardDialog = false;
    int32 SelectedRewardChoice = 0;

    FReply OnAcceptClicked();
    FReply OnDeclineClicked();
    FReply OnCompleteClicked();
    FReply OnRewardItemClicked(int32 RewardIndex);

    /** Generate quest text content */
    void RefreshContent();
};