#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

class UWowConnectionManager;

/**
 * Party invite dialog widget
 * Shows when a player invites you to a group with Accept/Decline buttons
 */
class WOWUNREAL_API SWowPartyInvite : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowPartyInvite)
        {}
        SLATE_ARGUMENT(UWowConnectionManager*, ConnectionManager)
        SLATE_ARGUMENT(FString, InviterName)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Close the dialog */
    void CloseDialog();

private:
    /** Connection manager for sending accept/decline */
    TWeakObjectPtr<UWowConnectionManager> ConnectionManager;

    /** Name of the player who invited us */
    FString InviterName;

    /** Accept button clicked */
    FReply OnAcceptClicked();

    /** Decline button clicked */
    FReply OnDeclineClicked();

    /** Get the invite message text */
    FText GetInviteMessageText() const;
};