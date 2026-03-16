#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "WowEntity.h"

class UWowConnectionManager;

/**
 * Party frame widget that shows group members and their health/mana
 * Displays vertically on the left side of the screen
 */
class WOWUNREAL_API SWowPartyFrame : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowPartyFrame)
        {}
        SLATE_ARGUMENT(UWowConnectionManager*, ConnectionManager)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Update party information from packet data */
    void UpdatePartyInfo(const FWowGroupInfo& GroupInfo);

    /** Update member health/mana from entity data */
    void UpdateMemberFromEntity(const FWowEntity& Entity);

    /** Clear all party information */
    void ClearParty();

private:
    struct FPartyMemberWidget
    {
        uint64 Guid = 0;
        FString Name;
        uint8 Level = 0;
        uint8 Class = 0;
        TSharedPtr<SBorder> MemberBorder;
        TSharedPtr<STextBlock> NameText;
        TSharedPtr<STextBlock> LevelText;
        TSharedPtr<SProgressBar> HealthBar;
        TSharedPtr<SProgressBar> PowerBar;
        int32 CurrentHealth = 0;
        int32 MaxHealth = 0;
        int32 CurrentPower = 0;
        int32 MaxPower = 0;
        bool bInRange = false;
    };

    /** Connection manager for sending group commands */
    TWeakObjectPtr<UWowConnectionManager> ConnectionManager;

    /** Current group information */
    FWowGroupInfo CurrentGroupInfo;

    /** Party member widgets */
    TArray<FPartyMemberWidget> MemberWidgets;

    /** Container for party member widgets */
    TSharedPtr<SVerticalBox> MembersContainer;

    /** Create a single member widget */
    TSharedRef<SWidget> CreateMemberWidget(const FWowGroupMember& Member);

    /** Update a member widget with entity data */
    void UpdateMemberWidget(FPartyMemberWidget& Widget);

    /** Get class color for health bars */
    FLinearColor GetClassColor(uint8 ClassId) const;

    /** Get power bar color based on class */
    FLinearColor GetPowerColor(uint8 ClassId) const;

    /** Get member visibility */
    EVisibility GetMemberVisibility() const;

    /** Get health bar percent */
    TOptional<float> GetHealthPercent(int32 MemberIndex) const;

    /** Get power bar percent */
    TOptional<float> GetPowerPercent(int32 MemberIndex) const;

    /** Get member name text */
    FText GetMemberNameText(int32 MemberIndex) const;

    /** Get member level text */
    FText GetMemberLevelText(int32 MemberIndex) const;
};