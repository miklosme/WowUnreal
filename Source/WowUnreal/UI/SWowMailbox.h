#pragma once
#include "CoreMinimal.h"
#include "WowNetwork/Public/WowEntity.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UWowConnectionManager;
class SScrollBox;

/**
 * Mailbox inbox widget
 */
class WOWUNREAL_API SWowMailbox : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowMailbox)
    {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UWowConnectionManager* InConnectionManager);

    /** Toggle visibility of the mailbox window */
    void ToggleVisibility();

    /** Check if mailbox is visible */
    bool IsVisible() const;

    /** Show the mailbox window and request inbox contents */
    void Show(uint64 MailboxGuid);

private:
    /** Handle close button clicked */
    FReply OnCloseButtonClicked();

    /** Refresh the inbox contents from the server */
    FReply OnRefreshButtonClicked();

    /** Request mail list from server */
    void RequestMailList();

    /** Receive a parsed mail list from the packet handler */
    void OnMailListReceived(const TArray<FWowMailMessage>& InMail);

    /** Rebuild the inbox rows */
    void RefreshInboxList();
    TSharedRef<SWidget> BuildMailRow(const FWowMailMessage& Mail) const;
    FString GetSenderLabel(const FWowMailMessage& Mail) const;
    FText GetMoneyText(uint32 Copper) const;

    TWeakObjectPtr<UWowConnectionManager> ConnectionManager;
    TArray<FWowMailMessage> CurrentMail;
    EVisibility CurrentVisibility = EVisibility::Hidden;
    uint64 CurrentMailboxGuid = 0;
    bool bWaitingForMailList = false;
    TSharedPtr<SScrollBox> MailList;
};
