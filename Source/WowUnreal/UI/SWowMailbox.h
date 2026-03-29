#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class FWowPacketHandler;

/**
 * Mailbox window widget (stub implementation)
 */
class WOWUNREAL_API SWowMailbox : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowMailbox)
    {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, FWowPacketHandler* InPacketHandler);

    /** Toggle visibility of the mailbox window */
    void ToggleVisibility();

    /** Check if mailbox is visible */
    bool IsVisible() const;

    /** Show the mailbox window */
    void Show();

private:
    /** Handle close button clicked */
    FReply OnCloseButtonClicked();

    /** Handle inbox tab clicked */
    FReply OnInboxTabClicked();

    /** Handle send tab clicked */
    FReply OnSendTabClicked();

    /** Request mail list from server */
    void RequestMailList();

    /** Get tab button colors */
    FSlateColor GetInboxTabColor() const;
    FSlateColor GetSendTabColor() const;

    /** Get content text based on active tab */
    FText GetContentText() const;

    mutable FWowPacketHandler* PacketHandler = nullptr;
    EVisibility CurrentVisibility = EVisibility::Hidden;
    int32 ActiveTab = 0; // 0 = inbox, 1 = send
};