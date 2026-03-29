#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

struct FWowGuildMember;
class FWowPacketHandler;

/**
 * Guild roster window widget
 */
class WOWUNREAL_API SWowGuildRoster : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowGuildRoster)
    {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, FWowPacketHandler* InPacketHandler);

    /** Update the guild roster from the packet handler */
    void UpdateGuildRoster();

    /** Toggle visibility of the guild roster window */
    void ToggleVisibility();

    /** Check if guild roster is visible */
    bool IsVisible() const;

private:
    /** Create guild member entry widget */
    TSharedRef<SWidget> CreateGuildMemberEntry(const FWowGuildMember& Member) const;

    /** Get class name from class ID */
    FString GetClassName(uint8 ClassId) const;

    /** Get zone name from zone ID */
    FString GetZoneName(uint32 ZoneId) const;

    /** Get online status text */
    FText GetOnlineStatusText(uint8 Status) const;

    /** Get online status color */
    FSlateColor GetOnlineStatusColor(uint8 Status) const;

    /** Handle close button clicked */
    FReply OnCloseButtonClicked();

    /** Request guild roster from server */
    void RequestGuildRoster();

    /** Get guild title with name */
    FText GetGuildTitle() const;

    mutable FWowPacketHandler* PacketHandler = nullptr;
    TSharedPtr<class SScrollBox> MemberListScrollBox;
    EVisibility CurrentVisibility = EVisibility::Hidden;
};