#include "SWowQuestLog.h"
#include "WowPacketHandler.h"
#include "WowEntity.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "WowQuestLog"

void SWowQuestLog::Construct(const FArguments& InArgs, FWowPacketHandler* InPacketHandler)
{
    PacketHandler = InPacketHandler;

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(350.0f)
        .HeightOverride(450.0f)
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
            .BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.9f))
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
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("QuestLogTitle", "Quest Log"))
                        .Font(FAppStyle::GetFontStyle("NormalFont"))
                        .ColorAndOpacity(FLinearColor(1.0f, 0.84f, 0.0f, 1.0f)) // Gold text
                        .Justification(ETextJustify::Center)
                    ]

                    // Close button
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .Text(FText::FromString(TEXT("X")))
                        .ToolTipText(LOCTEXT("CloseTooltip", "Close Quest Log"))
                        .OnClicked(this, &SWowQuestLog::OnCloseButtonClicked)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("X")))
                            .ColorAndOpacity(FLinearColor::White)
                        ]
                    ]
                ]

                // Quest list
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SAssignNew(QuestListScrollBox, SScrollBox)
                    .Orientation(Orient_Vertical)
                ]
            ]
        ]
    ];

    // Initialize visibility
    SetVisibility(CurrentVisibility);

    // Update quest list
    UpdateQuestList();
}

void SWowQuestLog::UpdateQuestList()
{
    if (!QuestListScrollBox.IsValid() || !PacketHandler)
    {
        return;
    }

    // Clear existing content
    QuestListScrollBox->ClearChildren();

    // Add each quest from the packet handler
    for (const FWowQuestLogEntry& QuestEntry : PacketHandler->QuestLog)
    {
        QuestListScrollBox->AddSlot()
        .Padding(0, 2, 0, 2)
        [
            CreateQuestEntry(QuestEntry)
        ];
    }

    // Show message if no quests
    if (PacketHandler->QuestLog.IsEmpty())
    {
        QuestListScrollBox->AddSlot()
        .Padding(0, 20, 0, 20)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("NoQuests", "No active quests"))
            .ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
            .Justification(ETextJustify::Center)
        ];
    }
}

TSharedRef<SWidget> SWowQuestLog::CreateQuestEntry(const FWowQuestLogEntry& QuestEntry) const
{
    FString QuestName = GetQuestName(QuestEntry.QuestId);
    FText StateText = GetQuestStateText(QuestEntry.State);
    FSlateColor StateColor = GetQuestStateColor(QuestEntry.State);

    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f))
        .Padding(6.0f)
        [
            SNew(SVerticalBox)

            // Quest name and state
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)

                // Quest name
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(QuestName))
                    .ColorAndOpacity(FLinearColor(1.0f, 0.84f, 0.0f, 1.0f)) // Gold text
                    .Font(FAppStyle::GetFontStyle("NormalFont"))
                ]

                // Quest state
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(8, 0, 0, 0)
                [
                    SNew(STextBlock)
                    .Text(StateText)
                    .ColorAndOpacity(StateColor)
                    .Font(FAppStyle::GetFontStyle("SmallFont"))
                ]
            ]

            // Quest objectives - create them directly here instead of using lambda
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 4, 0, 0)
            [
                CreateObjectivesWidget(QuestEntry)
            ]

            // Abandon button
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 4, 0, 0)
            .HAlign(HAlign_Right)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .Text(LOCTEXT("AbandonQuest", "Abandon"))
                .ToolTipText(LOCTEXT("AbandonTooltip", "Abandon this quest"))
                .OnClicked_Lambda([this, QuestId = QuestEntry.QuestId]() -> FReply
                {
                    return OnAbandonQuest(QuestId);
                })
            ]
        ];
}

TSharedRef<SWidget> SWowQuestLog::CreateObjectivesWidget(const FWowQuestLogEntry& QuestEntry) const
{
    TSharedRef<SVerticalBox> ObjectivesBox = SNew(SVerticalBox);

    if (QuestEntry.Objectives.Num() > 0)
    {
        for (int32 i = 0; i < QuestEntry.Objectives.Num(); ++i)
        {
            const FWowQuestObjective& Objective = QuestEntry.Objectives[i];

            // Skip empty objectives
            if (Objective.Required == 0)
                continue;

            FString ObjectiveText;
            if (Objective.CreatureOrGOId != 0)
            {
                ObjectiveText = FString::Printf(TEXT("• Kill %u: %u / %u"),
                    Objective.CreatureOrGOId, Objective.Count, Objective.Required);
            }
            else
            {
                ObjectiveText = FString::Printf(TEXT("• Objective %d: %u / %u"),
                    i + 1, Objective.Count, Objective.Required);
            }

            // Color based on completion
            FLinearColor ObjectiveColor = (Objective.Count >= Objective.Required) ?
                FLinearColor(0.2f, 1.0f, 0.2f, 1.0f) : // Green if complete
                FLinearColor(0.9f, 0.9f, 0.9f, 1.0f);  // White if incomplete

            ObjectivesBox->AddSlot()
            .AutoHeight()
            .Padding(10, 1, 0, 1)
            [
                SNew(STextBlock)
                .Text(FText::FromString(ObjectiveText))
                .ColorAndOpacity(ObjectiveColor)
                .Font(FAppStyle::GetFontStyle("SmallFont"))
            ];
        }
    }

    return ObjectivesBox;
}

FString SWowQuestLog::GetQuestName(uint32 QuestId) const
{
    // TODO: Look up quest name from QuestQueryResponse cache or DBC when available
    // For now, just return Quest #ID
    return FString::Printf(TEXT("Quest #%u"), QuestId);
}

FText SWowQuestLog::GetQuestStateText(uint32 State) const
{
    switch (State)
    {
        case 0: return LOCTEXT("InProgress", "In Progress");
        case 1: return LOCTEXT("Complete", "Complete");
        case 2: return LOCTEXT("Failed", "Failed");
        default: return LOCTEXT("Unknown", "Unknown");
    }
}

FSlateColor SWowQuestLog::GetQuestStateColor(uint32 State) const
{
    switch (State)
    {
        case 0: return FSlateColor(FLinearColor(1.0f, 1.0f, 0.3f, 1.0f)); // Yellow for in progress
        case 1: return FSlateColor(FLinearColor(0.2f, 1.0f, 0.2f, 1.0f)); // Green for complete
        case 2: return FSlateColor(FLinearColor(1.0f, 0.3f, 0.3f, 1.0f)); // Red for failed
        default: return FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f, 1.0f)); // Gray for unknown
    }
}

void SWowQuestLog::ToggleVisibility()
{
    if (CurrentVisibility == EVisibility::Hidden)
    {
        CurrentVisibility = EVisibility::SelfHitTestInvisible;
        SetVisibility(CurrentVisibility);
        UpdateQuestList(); // Refresh data when showing
    }
    else
    {
        CurrentVisibility = EVisibility::Hidden;
        SetVisibility(CurrentVisibility);
    }
}

bool SWowQuestLog::IsVisible() const
{
    return CurrentVisibility != EVisibility::Hidden;
}

FReply SWowQuestLog::OnCloseButtonClicked()
{
    CurrentVisibility = EVisibility::Hidden;
    SetVisibility(CurrentVisibility);
    return FReply::Handled();
}

FReply SWowQuestLog::OnAbandonQuest(uint32 QuestId) const
{
    // TODO: Send CMSG_QUESTLOG_ABANDON_QUEST packet to server
    // For now, just remove from local display
    if (PacketHandler)
    {
        PacketHandler->QuestLog.RemoveAll([QuestId](const FWowQuestLogEntry& Entry)
        {
            return Entry.QuestId == QuestId;
        });

        // Refresh the display (cast away const since we need to modify the UI)
        const_cast<SWowQuestLog*>(this)->UpdateQuestList();
    }

    return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE