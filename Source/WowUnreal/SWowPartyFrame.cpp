#include "SWowPartyFrame.h"
#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Styling/AppStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowPartyFrame, Log, All);

void SWowPartyFrame::Construct(const FArguments& InArgs)
{
    ConnectionManager = InArgs._ConnectionManager;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f))
        .Padding(8.0f)
        .Visibility(this, &SWowPartyFrame::GetMemberVisibility)
        [
            SAssignNew(MembersContainer, SVerticalBox)
        ]
    ];
}

void SWowPartyFrame::UpdatePartyInfo(const FWowGroupInfo& GroupInfo)
{
    CurrentGroupInfo = GroupInfo;
    MemberWidgets.Empty();
    MembersContainer->ClearChildren();

    if (GroupInfo.IsEmpty())
    {
        return;
    }

    UE_LOG(LogWowPartyFrame, Log, TEXT("Updating party with %d members"), GroupInfo.MemberCount);

    // Create widgets for each member (excluding self)
    for (const FWowGroupMember& Member : GroupInfo.Members)
    {
        // Skip local player - we don't show ourselves in party frames
        if (ConnectionManager.IsValid() && ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid == Member.Guid)
        {
            continue;
        }

        FPartyMemberWidget MemberWidget;
        MemberWidget.Guid = Member.Guid;
        MemberWidget.Name = Member.Name;
        MemberWidget.Level = Member.Level;
        MemberWidget.Class = Member.Class;
        MemberWidget.CurrentHealth = 1;
        MemberWidget.MaxHealth = 1;
        MemberWidget.CurrentPower = 1;
        MemberWidget.MaxPower = 1;

        const int32 MemberIndex = MemberWidgets.Num();
        MemberWidgets.Add(MemberWidget);

        TSharedRef<SWidget> MemberWidgetRef = CreateMemberWidget(Member);
        MembersContainer->AddSlot()
            .Padding(0, 2)
            .AutoHeight()
            [
                MemberWidgetRef
            ];
    }

    // Try to update with existing entity data
    if (ConnectionManager.IsValid())
    {
        for (const auto& EntityPair : ConnectionManager->PacketHandler.EntityManager.GetAll())
        {
            UpdateMemberFromEntity(*EntityPair.Value);
        }
    }
}

void SWowPartyFrame::UpdateMemberFromEntity(const FWowEntity& Entity)
{
    if (!Entity.IsUnit())
    {
        return;
    }

    for (FPartyMemberWidget& Widget : MemberWidgets)
    {
        if (Widget.Guid == Entity.Guid)
        {
            Widget.CurrentHealth = Entity.GetHealth();
            Widget.MaxHealth = Entity.GetMaxHealth();
            Widget.CurrentPower = Entity.IsUnit() ? static_cast<const FWowUnitEntity&>(Entity).GetPower(0) : 0;
            Widget.MaxPower = Entity.IsUnit() ? static_cast<const FWowUnitEntity&>(Entity).GetMaxPower(0) : 0;
            Widget.bInRange = true;

            UpdateMemberWidget(Widget);
            break;
        }
    }
}

void SWowPartyFrame::ClearParty()
{
    CurrentGroupInfo.Clear();
    MemberWidgets.Empty();
    MembersContainer->ClearChildren();
}

TSharedRef<SWidget> SWowPartyFrame::CreateMemberWidget(const FWowGroupMember& Member)
{
    const int32 MemberIndex = MemberWidgets.Num() - 1;
    FPartyMemberWidget& Widget = MemberWidgets[MemberIndex];

    return SAssignNew(Widget.MemberBorder, SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
        .BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.9f))
        .Padding(4.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SAssignNew(Widget.NameText, STextBlock)
                    .Text(this, &SWowPartyFrame::GetMemberNameText, MemberIndex)
                    .ColorAndOpacity(GetClassColor(Member.Class))
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SAssignNew(Widget.LevelText, STextBlock)
                    .Text(this, &SWowPartyFrame::GetMemberLevelText, MemberIndex)
                    .ColorAndOpacity(FLinearColor::White)
                ]
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 2)
            [
                SAssignNew(Widget.HealthBar, SProgressBar)
                .Percent(this, &SWowPartyFrame::GetHealthPercent, MemberIndex)
                .FillColorAndOpacity(FLinearColor::Green)
                .BackgroundImage(FAppStyle::GetBrush("ProgressBar.Background"))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 1, 0, 0)
            [
                SAssignNew(Widget.PowerBar, SProgressBar)
                .Percent(this, &SWowPartyFrame::GetPowerPercent, MemberIndex)
                .FillColorAndOpacity(GetPowerColor(Member.Class))
                .BackgroundImage(FAppStyle::GetBrush("ProgressBar.Background"))
            ]
        ];
}

void SWowPartyFrame::UpdateMemberWidget(FPartyMemberWidget& Widget)
{
    // Widget updates are handled by the slate attribute bindings automatically
    // No manual updates needed here
}

FLinearColor SWowPartyFrame::GetClassColor(uint8 ClassId) const
{
    // WoW 3.3.5 class colors
    switch (ClassId)
    {
    case 1: return FLinearColor(0.78f, 0.61f, 0.43f); // Warrior - tan
    case 2: return FLinearColor(0.96f, 0.55f, 0.73f); // Paladin - pink
    case 3: return FLinearColor(0.67f, 0.83f, 0.45f); // Hunter - green
    case 4: return FLinearColor(1.0f, 0.96f, 0.41f);  // Rogue - yellow
    case 5: return FLinearColor(1.0f, 1.0f, 1.0f);    // Priest - white
    case 6: return FLinearColor(0.77f, 0.12f, 0.23f); // Death Knight - red
    case 7: return FLinearColor(0.0f, 0.44f, 0.87f);  // Shaman - blue
    case 8: return FLinearColor(0.41f, 0.8f, 0.94f);  // Mage - light blue
    case 9: return FLinearColor(0.58f, 0.51f, 0.79f); // Warlock - purple
    case 11: return FLinearColor(1.0f, 0.49f, 0.04f); // Druid - orange
    default: return FLinearColor::White;
    }
}

FLinearColor SWowPartyFrame::GetPowerColor(uint8 ClassId) const
{
    // Power bar colors based on class primary power type
    switch (ClassId)
    {
    case 1:  // Warrior - rage (red)
    case 6:  // Death Knight - runic power (cyan-ish)
        return FLinearColor(1.0f, 0.0f, 0.0f);
    case 3:  // Hunter - focus/mana (blue)
    case 4:  // Rogue - energy (yellow)
        return FLinearColor(1.0f, 1.0f, 0.0f);
    default: // Mana users (blue)
        return FLinearColor(0.0f, 0.0f, 1.0f);
    }
}

EVisibility SWowPartyFrame::GetMemberVisibility() const
{
    return (!CurrentGroupInfo.IsEmpty() && MemberWidgets.Num() > 0) ? EVisibility::Visible : EVisibility::Collapsed;
}

TOptional<float> SWowPartyFrame::GetHealthPercent(int32 MemberIndex) const
{
    if (!MemberWidgets.IsValidIndex(MemberIndex))
    {
        return 0.0f;
    }

    const FPartyMemberWidget& Widget = MemberWidgets[MemberIndex];
    if (Widget.MaxHealth <= 0)
    {
        return 0.0f;
    }

    return static_cast<float>(Widget.CurrentHealth) / static_cast<float>(Widget.MaxHealth);
}

TOptional<float> SWowPartyFrame::GetPowerPercent(int32 MemberIndex) const
{
    if (!MemberWidgets.IsValidIndex(MemberIndex))
    {
        return 0.0f;
    }

    const FPartyMemberWidget& Widget = MemberWidgets[MemberIndex];
    if (Widget.MaxPower <= 0)
    {
        return 0.0f;
    }

    return static_cast<float>(Widget.CurrentPower) / static_cast<float>(Widget.MaxPower);
}

FText SWowPartyFrame::GetMemberNameText(int32 MemberIndex) const
{
    if (!MemberWidgets.IsValidIndex(MemberIndex))
    {
        return FText::GetEmpty();
    }

    return FText::FromString(MemberWidgets[MemberIndex].Name);
}

FText SWowPartyFrame::GetMemberLevelText(int32 MemberIndex) const
{
    if (!MemberWidgets.IsValidIndex(MemberIndex))
    {
        return FText::GetEmpty();
    }

    const FPartyMemberWidget& Widget = MemberWidgets[MemberIndex];
    return FText::FromString(FString::Printf(TEXT("%d"), Widget.Level));
}