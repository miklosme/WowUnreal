#include "SWowTalentWindow.h"
#include "WowNetwork/Public/WowConnectionManager.h"
#include "WowNetwork/Public/WowPacketHandler.h"
#include "WowNetwork/Public/WowEntity.h"
#include "WowData/Public/Formats/Dbc/DbcStore.h"
#include "WowData/Public/Formats/Dbc/TalentDbc.h"
#include "WowData/Public/Formats/Dbc/TalentTabDbc.h"
#include "WowData/Public/Formats/Dbc/SpellDbc.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Engine/Engine.h"

#define LOCTEXT_NAMESPACE "WowTalentWindow"

void SWowTalentWindow::Construct(const FArguments& InArgs, UWowConnectionManager* InConnectionManager)
{
    ConnectionManager = InConnectionManager;
    CurrentTalentTab = 0;

    // Bind to talents updated event
    if (ConnectionManager.IsValid())
    {
        ConnectionManager->PacketHandler.OnTalentsUpdated.AddSP(this, &SWowTalentWindow::OnTalentsUpdated);
    }

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(500.0f)
        .HeightOverride(550.0f)
        .Visibility_Lambda([this]() { return CurrentVisibility; })
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
            .Padding(8.0f)
            [
                SNew(SVerticalBox)

                // Title bar with close button
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    SNew(SHorizontalBox)

                    // Title
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("TalentWindowTitle", "Talents"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                        .ColorAndOpacity(FLinearColor::White)
                    ]

                    // Close button
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .ButtonStyle(FCoreStyle::Get(), "Button")
                        .ContentPadding(FMargin(8, 4))
                        .Text(LOCTEXT("Close", "X"))
                        .OnClicked(this, &SWowTalentWindow::OnCloseButtonClicked)
                    ]
                ]

                // Tab buttons
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    CreateTabButtons()
                ]

                // Talent grid
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                .Padding(0, 0, 0, 8)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()
                    [
                        SAssignNew(TalentGrid, SUniformGridPanel)
                    ]
                ]

                // Available talent points
                + SVerticalBox::Slot()
                .AutoHeight()
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .HAlign(HAlign_Center)
                    [
                        SAssignNew(TalentPointsText, STextBlock)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                        .ColorAndOpacity(FLinearColor::Yellow)
                        .Text_Lambda([this]()
                        {
                            int32 AvailablePoints = GetAvailableTalentPoints();
                            return FText::FromString(FString::Printf(TEXT("Available Talent Points: %d"), AvailablePoints));
                        })
                    ]
                ]
            ]
        ]
    ];

    // Initialize talent data
    RefreshTalentData();
}

void SWowTalentWindow::ToggleVisibility()
{
    if (CurrentVisibility == EVisibility::Hidden)
    {
        CurrentVisibility = EVisibility::Visible;
        RefreshTalentData();
    }
    else
    {
        CurrentVisibility = EVisibility::Hidden;
    }
}

bool SWowTalentWindow::IsVisible() const
{
    return CurrentVisibility == EVisibility::Visible;
}

void SWowTalentWindow::RefreshTalentData()
{
    if (TalentGrid.IsValid())
    {
        TalentGrid->ClearChildren();

        // Rebuild talent grid for current tab
        for (int32 Row = 0; Row < 11; ++Row)
        {
            for (int32 Col = 0; Col < 4; ++Col)
            {
                TalentGrid->AddSlot(Col, Row)
                [
                    CreateTalentSlot(Row, Col)
                ];
            }
        }
    }

    // Refresh tab buttons
    CreateTabButtons();
}

uint8 SWowTalentWindow::GetPlayerClass() const
{
    if (!ConnectionManager.IsValid())
        return 0;

    const FWowPlayerEntity* LocalPlayer = ConnectionManager->PacketHandler.EntityManager.GetLocalPlayer();
    if (!LocalPlayer)
        return 0;

    return LocalPlayer->GetClassId();
}

TArray<const FTalentTabDbcEntry*> SWowTalentWindow::GetPlayerTalentTabs() const
{
    TArray<const FTalentTabDbcEntry*> PlayerTabs;

    uint8 PlayerClass = GetPlayerClass();
    if (PlayerClass == 0)
        return PlayerTabs;

    uint32 ClassMask = 1 << (PlayerClass - 1);

    const FTalentTabDbc& TalentTabDbc = FDbcStore::Get().TalentTabs();
    for (const FTalentTabDbcEntry& TabEntry : TalentTabDbc.GetAll())
    {
        if ((TabEntry.ClassMask & ClassMask) != 0)
        {
            PlayerTabs.Add(&TabEntry);
        }
    }

    // Sort by TabPage
    PlayerTabs.Sort([](const FTalentTabDbcEntry& A, const FTalentTabDbcEntry& B)
    {
        return A.TabPage < B.TabPage;
    });

    return PlayerTabs;
}

TSharedRef<SWidget> SWowTalentWindow::CreateTabButtons()
{
    TSharedRef<SHorizontalBox> TabBox = SNew(SHorizontalBox);

    TArray<const FTalentTabDbcEntry*> PlayerTabs = GetPlayerTalentTabs();
    TabButtons.Empty();

    for (int32 i = 0; i < PlayerTabs.Num(); ++i)
    {
        const FTalentTabDbcEntry* TabEntry = PlayerTabs[i];
        if (!TabEntry)
            continue;

        TSharedPtr<SButton> TabButton;

        TabBox->AddSlot()
        .FillWidth(1.0f)
        .Padding(2, 0)
        [
            SAssignNew(TabButton, SButton)
            .ButtonStyle(FCoreStyle::Get(), "Button")
            .ContentPadding(FMargin(8, 6))
            .OnClicked(this, &SWowTalentWindow::OnTabButtonClicked, i)
            .ButtonColorAndOpacity_Lambda([this, i]()
            {
                return (CurrentTalentTab == i) ? FLinearColor(0.7f, 0.7f, 0.2f, 1.0f) : FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);
            })
            [
                SNew(STextBlock)
                .Text(FText::FromString(TabEntry->Name))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor::White)
                .Justification(ETextJustify::Center)
            ]
        ];

        TabButtons.Add(TabButton);
    }

    return TabBox;
}

TSharedRef<SWidget> SWowTalentWindow::CreateTalentGrid()
{
    return SAssignNew(TalentGrid, SUniformGridPanel);
}

TSharedRef<SWidget> SWowTalentWindow::CreateTalentSlot(int32 Row, int32 Col)
{
    const FTalentDbcEntry* TalentEntry = GetTalentAt(Row, Col);

    if (!TalentEntry)
    {
        // Empty slot
        return SNew(SBox)
        .WidthOverride(40.0f)
        .HeightOverride(40.0f)
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
            .BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f))
        ];
    }

    uint8 CurrentRank = GetTalentRank(TalentEntry->TalentID);
    uint8 MaxRank = 5; // Default max rank

    // Find actual max rank by checking which RankID slots are filled
    for (int32 i = 4; i >= 0; --i)
    {
        if (TalentEntry->RankID[i] != 0)
        {
            MaxRank = i + 1;
            break;
        }
    }

    FLinearColor TalentColor = GetTalentColor(TalentEntry, CurrentRank);
    FText DisplayText = GetTalentDisplayText(TalentEntry, CurrentRank);
    FText TooltipText = GetTalentTooltip(TalentEntry);

    return SNew(SBox)
    .WidthOverride(40.0f)
    .HeightOverride(40.0f)
    .ToolTip(
        SNew(SToolTip)
        [
            SNew(STextBlock)
            .Text(TooltipText)
            .WrapTextAt(300.0f)
        ]
    )
    [
        SNew(SButton)
        .ButtonStyle(FCoreStyle::Get(), "Button")
        .ButtonColorAndOpacity(TalentColor)
        .ContentPadding(FMargin(2))
        .OnClicked(this, &SWowTalentWindow::OnTalentClicked, TalentEntry->TalentID)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .HAlign(HAlign_Center)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Text(DisplayText)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                .ColorAndOpacity(FLinearColor::White)
                .Justification(ETextJustify::Center)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("%d/%d"), CurrentRank, MaxRank)))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 7))
                .ColorAndOpacity(FLinearColor::Yellow)
            ]
        ]
    ];
}

const FTalentDbcEntry* SWowTalentWindow::GetTalentAt(int32 Row, int32 Col) const
{
    TArray<const FTalentTabDbcEntry*> PlayerTabs = GetPlayerTalentTabs();
    if (!PlayerTabs.IsValidIndex(CurrentTalentTab))
        return nullptr;

    uint32 TalentTabID = PlayerTabs[CurrentTalentTab]->TalentTabID;

    const FTalentDbc& TalentDbc = FDbcStore::Get().Talents();
    for (const FTalentDbcEntry& TalentEntry : TalentDbc.GetAll())
    {
        if (TalentEntry.TalentTab == TalentTabID &&
            TalentEntry.Row == static_cast<uint32>(Row) &&
            TalentEntry.Col == static_cast<uint32>(Col))
        {
            return &TalentEntry;
        }
    }

    return nullptr;
}

uint8 SWowTalentWindow::GetTalentRank(uint32 TalentId) const
{
    if (!ConnectionManager.IsValid())
        return 0;

    for (const FWowTalentInfo& TalentInfo : ConnectionManager->PacketHandler.Talents)
    {
        if (TalentInfo.TalentId == TalentId)
        {
            return TalentInfo.Rank;
        }
    }

    return 0;
}

bool SWowTalentWindow::CanLearnTalent(const FTalentDbcEntry* TalentEntry) const
{
    if (!TalentEntry)
        return false;

    uint8 CurrentRank = GetTalentRank(TalentEntry->TalentID);

    // Find max rank
    uint8 MaxRank = 5;
    for (int32 i = 4; i >= 0; --i)
    {
        if (TalentEntry->RankID[i] != 0)
        {
            MaxRank = i + 1;
            break;
        }
    }

    // Already at max rank
    if (CurrentRank >= MaxRank)
        return false;

    // Check if we have available talent points
    if (GetAvailableTalentPoints() <= 0)
        return false;

    // Check dependencies
    if (TalentEntry->DependsOn != 0)
    {
        uint8 DependencyRank = GetTalentRank(TalentEntry->DependsOn);
        if (DependencyRank < TalentEntry->DependsOnRank)
            return false;
    }

    // Check tier requirements (need to spend points in previous tiers)
    uint32 RequiredPointsInTab = TalentEntry->Row * 5; // 5 points per tier
    uint32 PointsSpentInTab = GetPointsSpentInTab(CurrentTalentTab);

    return PointsSpentInTab >= RequiredPointsInTab;
}

int32 SWowTalentWindow::GetAvailableTalentPoints() const
{
    if (!ConnectionManager.IsValid())
        return 0;

    const FWowPlayerEntity* LocalPlayer = ConnectionManager->PacketHandler.EntityManager.GetLocalPlayer();
    if (!LocalPlayer)
        return 0;

    // Calculate total talent points (1 per level starting at level 10)
    uint32 PlayerLevel = LocalPlayer->GetLevel();
    int32 TotalTalentPoints = FMath::Max(0, static_cast<int32>(PlayerLevel) - 9);

    // Calculate used talent points
    int32 UsedTalentPoints = 0;
    for (const FWowTalentInfo& TalentInfo : ConnectionManager->PacketHandler.Talents)
    {
        UsedTalentPoints += TalentInfo.Rank;
    }

    return TotalTalentPoints - UsedTalentPoints;
}

int32 SWowTalentWindow::GetPointsSpentInTab(int32 TabIndex) const
{
    TArray<const FTalentTabDbcEntry*> PlayerTabs = GetPlayerTalentTabs();
    if (!PlayerTabs.IsValidIndex(TabIndex))
        return 0;

    uint32 TalentTabID = PlayerTabs[TabIndex]->TalentTabID;

    int32 PointsSpent = 0;
    const FTalentDbc& TalentDbc = FDbcStore::Get().Talents();

    for (const FWowTalentInfo& TalentInfo : ConnectionManager->PacketHandler.Talents)
    {
        const FTalentDbcEntry* TalentEntry = TalentDbc.GetById(TalentInfo.TalentId);
        if (TalentEntry && TalentEntry->TalentTab == TalentTabID)
        {
            PointsSpent += TalentInfo.Rank;
        }
    }

    return PointsSpent;
}

FReply SWowTalentWindow::OnTabButtonClicked(int32 TabIndex)
{
    CurrentTalentTab = TabIndex;
    RefreshTalentData();
    return FReply::Handled();
}

FReply SWowTalentWindow::OnTalentClicked(uint32 TalentId)
{
    const FTalentDbc& TalentDbc = FDbcStore::Get().Talents();
    const FTalentDbcEntry* TalentEntry = TalentDbc.GetById(TalentId);

    if (!TalentEntry || !CanLearnTalent(TalentEntry))
        return FReply::Handled();

    // Send learn talent packet
    if (ConnectionManager.IsValid())
    {
        uint8 CurrentRank = GetTalentRank(TalentId);
        uint8 RequestedRank = CurrentRank + 1;

        ConnectionManager->SendLearnTalent(TalentId, RequestedRank);

        UE_LOG(LogTemp, Log, TEXT("Learning talent %u rank %u"), TalentId, RequestedRank);
    }

    return FReply::Handled();
}

FReply SWowTalentWindow::OnCloseButtonClicked()
{
    CurrentVisibility = EVisibility::Hidden;
    return FReply::Handled();
}

FLinearColor SWowTalentWindow::GetTalentColor(const FTalentDbcEntry* TalentEntry, uint8 CurrentRank) const
{
    if (!TalentEntry)
        return FLinearColor(0.2f, 0.2f, 0.2f, 1.0f); // Dark grey for empty

    if (CurrentRank > 0)
    {
        return FLinearColor(1.0f, 0.8f, 0.2f, 1.0f); // Gold for learned talents
    }
    else if (CanLearnTalent(TalentEntry))
    {
        return FLinearColor(0.2f, 0.8f, 0.2f, 1.0f); // Green for learnable
    }
    else
    {
        return FLinearColor(0.4f, 0.4f, 0.4f, 1.0f); // Grey for locked
    }
}

FText SWowTalentWindow::GetTalentTooltip(const FTalentDbcEntry* TalentEntry) const
{
    if (!TalentEntry)
        return FText::GetEmpty();

    uint8 CurrentRank = GetTalentRank(TalentEntry->TalentID);
    const FSpellDbcEntry* SpellData = GetTalentSpellData(TalentEntry, CurrentRank + 1);

    FString TooltipText;

    if (SpellData)
    {
        TooltipText = SpellData->SpellName;

        if (!SpellData->Description.IsEmpty())
        {
            TooltipText += FString::Printf(TEXT("\n\n%s"), *SpellData->Description);
        }
    }
    else
    {
        TooltipText = FString::Printf(TEXT("Talent ID: %u"), TalentEntry->TalentID);
    }

    // Add requirement info
    if (TalentEntry->DependsOn != 0)
    {
        TooltipText += FString::Printf(TEXT("\n\nRequires: Talent %u (Rank %u)"),
                                     TalentEntry->DependsOn, TalentEntry->DependsOnRank);
    }

    uint32 RequiredPointsInTab = TalentEntry->Row * 5;
    if (RequiredPointsInTab > 0)
    {
        TooltipText += FString::Printf(TEXT("\nRequires %u points in this talent tree"), RequiredPointsInTab);
    }

    return FText::FromString(TooltipText);
}

const FSpellDbcEntry* SWowTalentWindow::GetTalentSpellData(const FTalentDbcEntry* TalentEntry, uint8 Rank) const
{
    if (!TalentEntry || Rank == 0 || Rank > 5)
        return nullptr;

    uint32 SpellId = TalentEntry->RankID[Rank - 1];
    if (SpellId == 0)
        return nullptr;

    return FDbcStore::Get().Spells().GetById(SpellId);
}

FText SWowTalentWindow::GetTalentDisplayText(const FTalentDbcEntry* TalentEntry, uint8 CurrentRank) const
{
    if (!TalentEntry)
        return FText::GetEmpty();

    const FSpellDbcEntry* SpellData = GetTalentSpellData(TalentEntry, FMath::Max(1, static_cast<int32>(CurrentRank)));

    if (SpellData)
    {
        // Show abbreviated spell name
        FString SpellName = SpellData->SpellName;
        if (SpellName.Len() > 8)
        {
            SpellName = SpellName.Left(6) + TEXT("..");
        }
        return FText::FromString(SpellName);
    }

    return FText::FromString(FString::Printf(TEXT("T%u"), TalentEntry->TalentID));
}

void SWowTalentWindow::OnTalentsUpdated()
{
    // Refresh the talent display when talent data is updated from the server
    RefreshTalentData();
}

#undef LOCTEXT_NAMESPACE