#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "WowEntity.h"

class SWrapBox;
class SVerticalBox;
class UWowConnectionManager;

class WOWUNREAL_API SWowPartyFrame : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowPartyFrame)
        {}
        SLATE_ARGUMENT(UWowConnectionManager*, ConnectionManager)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void UpdatePartyInfo(const FWowGroupInfo& GroupInfo);
    void UpdateMemberFromEntity(const FWowEntity& Entity);
    void ClearParty();

private:
    struct FPartyMemberWidget
    {
        uint64 Guid = 0;
        FString Name;
        uint8 Level = 0;
        uint8 Class = 0;
        uint8 Subgroup = 0;
        uint8 Flags = 0;
        uint8 Roles = 0;
        uint8 Status = WowGroupMemberStatus::ONLINE;
        bool bIsLocalPlayer = false;
        bool bIsLeader = false;
        TSharedPtr<SBorder> MemberBorder;
        TSharedPtr<STextBlock> NameText;
        TSharedPtr<STextBlock> LevelText;
        TSharedPtr<SProgressBar> HealthBar;
        TSharedPtr<SProgressBar> PowerBar;
        int32 CurrentHealth = 1;
        int32 MaxHealth = 1;
        int32 CurrentPower = 1;
        int32 MaxPower = 1;
        bool bInRange = false;
    };

    TSharedRef<SWidget> CreateMemberWidget(const FWowGroupMember& Member);
    void UpdateMemberWidget(FPartyMemberWidget& Widget);
    FWowGroupMember BuildLocalMember() const;

    FString GetRaidMarkLabel(int32 IconIndex) const;
    FString GetReadyStateLabel(const FPartyMemberWidget& Widget) const;
    FString GetMemberMetaLabel(const FPartyMemberWidget& Widget) const;
    bool CanLocalPlayerControlRaid() const;
    bool HasPendingReadyCheckResponse() const;

    FReply OnStartReadyCheckClicked();
    FReply OnReadyClicked();
    FReply OnNotReadyClicked();
    FReply OnRefreshRaidMarksClicked();
    FReply OnRaidMarkClicked(int32 IconIndex);

    FLinearColor GetClassColor(uint8 ClassId) const;
    FLinearColor GetPowerColor(uint8 ClassId) const;
    FLinearColor GetMemberTint(int32 MemberIndex) const;
    EVisibility GetMemberVisibility() const;
    EVisibility GetRaidControlsVisibility() const;
    EVisibility GetReadyCheckStartVisibility() const;
    EVisibility GetReadyCheckResponseVisibility() const;
    FText GetFrameTitleText() const;
    FText GetFrameStatusText() const;
    TOptional<float> GetHealthPercent(int32 MemberIndex) const;
    TOptional<float> GetPowerPercent(int32 MemberIndex) const;
    FText GetMemberNameText(int32 MemberIndex) const;
    FText GetMemberLevelText(int32 MemberIndex) const;
    FText GetMemberMetaText(int32 MemberIndex) const;

    TWeakObjectPtr<UWowConnectionManager> ConnectionManager;
    FWowGroupInfo CurrentGroupInfo;
    TArray<FPartyMemberWidget> MemberWidgets;
    TSharedPtr<SVerticalBox> MembersContainer;
    TSharedPtr<SWrapBox> RaidMarkButtons;
};
