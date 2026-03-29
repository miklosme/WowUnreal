#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UWowConnectionManager;

class WOWUNREAL_API SWowDuelInvite : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowDuelInvite)
        : _ConnectionManager(nullptr)
        , _ArbiterGuid(0)
    {}
        SLATE_ARGUMENT(UWowConnectionManager*, ConnectionManager)
        SLATE_ARGUMENT(FString, ChallengerName)
        SLATE_ARGUMENT(int64, ArbiterGuid)
        SLATE_EVENT(FSimpleDelegate, OnClosed)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    void CloseDialog();

private:
    TWeakObjectPtr<UWowConnectionManager> ConnectionManager;
    FString ChallengerName;
    int64 ArbiterGuid = 0;
    FSimpleDelegate OnClosed;

    FReply OnAcceptClicked();
    FReply OnDeclineClicked();
    FText GetInviteMessageText() const;
};
