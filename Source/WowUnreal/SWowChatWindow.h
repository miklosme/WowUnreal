#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UWowConnectionManager;

/**
 * Chat message entry with channel information and timestamp
 */
struct FWowChatMessage
{
    FString Message;
    FLinearColor Color;
    FString Timestamp;
    uint8 Type;
    FString SenderName;
    FString Channel;

    FWowChatMessage() = default;
    FWowChatMessage(const FString& InMessage, const FLinearColor& InColor, uint8 InType = 0,
                    const FString& InSenderName = TEXT(""), const FString& InChannel = TEXT(""))
        : Message(InMessage)
        , Color(InColor)
        , Type(InType)
        , SenderName(InSenderName)
        , Channel(InChannel)
    {
        // Generate timestamp [HH:MM]
        FDateTime Now = FDateTime::Now();
        Timestamp = FString::Printf(TEXT("[%02d:%02d]"), Now.GetHour(), Now.GetMinute());
    }
};

/**
 * Chat window widget with input box, channel tabs, and scrollable message history.
 * Replaces/augments the combat log to show both chat and combat messages.
 */
class WOWUNREAL_API SWowChatWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowChatWindow) {}
    SLATE_ARGUMENT(UWowConnectionManager*, ConnectionManager)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Add a chat message with automatic color coding by type */
    void AddChatMessage(uint8 Type, uint32 Language, uint64 SenderGuid, const FString& SenderName,
                       const FString& Message, const FString& Channel = TEXT(""));

    /** Add a combat/system message with explicit color */
    void AddCombatMessage(const FString& Message, const FLinearColor& Color);

    /** Clear all messages in current tab */
    void ClearMessages();

    /** Toggle input focus (Enter key) */
    void ToggleInputFocus();

    /** Check if input has focus */
    bool HasInputFocus() const;

    /** Set input mode for controller switching */
    DECLARE_DELEGATE_OneParam(FOnInputModeChanged, bool /* bGameAndUI */);
    FOnInputModeChanged OnInputModeChanged;

private:
    /** Channel tabs */
    enum class EChatTab : uint8
    {
        General = 0,
        Combat = 1,
        Trade = 2
    };

    /** Current active tab */
    EChatTab ActiveTab = EChatTab::General;

    /** Maximum number of messages per tab */
    static constexpr int32 MaxMessages = 200;

    /** Get color for chat message type */
    static FLinearColor GetColorForChatType(uint8 Type);

    /** Format chat message for display */
    FString FormatChatMessage(const FWowChatMessage& ChatMsg) const;

    /** Send chat message to server */
    void SendChatMessage();

    /** Parse chat command and determine type/target */
    struct FChatCommand
    {
        uint8 Type = 1; // SAY
        FString Target; // For whispers
        FString Message;
        FString Channel; // For channel messages
    };
    FChatCommand ParseChatCommand(const FString& Input) const;

    /** Get default language for player */
    uint32 GetDefaultLanguage() const { return 7; } // Common for now

    /** Handle input key events */
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual bool SupportsKeyboardFocus() const override { return true; }

    // UI event handlers
    FReply OnTabClicked(EChatTab Tab);
    void OnInputTextCommitted(const FText& Text, ETextCommit::Type CommitType);
    void OnInputTextChanged(const FText& Text);

    // UI elements
    TSharedPtr<class SScrollBox> ScrollBox;
    TSharedPtr<class SVerticalBox> MessageContainer;
    TSharedPtr<class SEditableTextBox> InputTextBox;
    TSharedPtr<class SHorizontalBox> TabContainer;

    // Message storage per tab
    TArray<FWowChatMessage> GeneralMessages;
    TArray<FWowChatMessage> CombatMessages;
    TArray<FWowChatMessage> TradeMessages;

    /** Get current tab's message array */
    TArray<FWowChatMessage>& GetCurrentMessages();
    const TArray<FWowChatMessage>& GetCurrentMessages() const;

    /** Rebuild message display for current tab */
    void RebuildMessageDisplay();

    /** Auto-scroll to bottom */
    void ScrollToBottom();

    /** Connection manager for sending messages */
    UWowConnectionManager* ConnectionManager = nullptr;

    /** Track input focus state */
    bool bInputHasFocus = false;

    /** Update tab button styles */
    void UpdateTabStyles();
    TSharedPtr<class SButton> GeneralTabButton;
    TSharedPtr<class SButton> CombatTabButton;
    TSharedPtr<class SButton> TradeTabButton;
};