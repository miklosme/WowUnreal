#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Combat log widget that displays scrolling combat messages.
 * Positioned in the bottom-left corner like the original WoW combat log.
 */
class SWowCombatLog : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowCombatLog) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Add a new combat message with color coding */
    void AddCombatMessage(const FString& Message, const FLinearColor& Color);

    /** Clear all messages */
    void ClearMessages();

private:
    /** Maximum number of messages to keep in memory */
    static constexpr int32 MaxMessages = 100;

    /** Scroll to bottom automatically when new messages arrive */
    void ScrollToBottom();

    TSharedPtr<class SScrollBox> ScrollBox;
    TSharedPtr<class SVerticalBox> MessageContainer;

    /** Keep track of messages for pruning old ones */
    int32 MessageCount = 0;
};