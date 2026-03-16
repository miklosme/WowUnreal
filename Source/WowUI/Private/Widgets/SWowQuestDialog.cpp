#include "Widgets/SWowQuestDialog.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Styling/CoreStyle.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowQuestDialog, Log, All);

void SWowQuestDialog::Construct(const FArguments& InArgs)
{
    OnQuestAccept = InArgs._OnQuestAccept;
    OnQuestComplete = InArgs._OnQuestComplete;
    OnQuestDecline = InArgs._OnQuestDecline;

    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.95f)) // Dark background
        .BorderImage(FCoreStyle::Get().GetBrush("Border"))
        .Padding(12.0f)
        [
            SNew(SVerticalBox)

            // Quest content (scrollable)
            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(ContentArea, SScrollBox)
                .Orientation(Orient_Vertical)
                .ScrollBarAlwaysVisible(false)
            ]

            // Rewards area
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f)
            [
                SAssignNew(RewardArea, SHorizontalBox)
            ]

            // Button area
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
                .Padding(FMargin(0.0f, 8.0f))
                .HAlign(HAlign_Center)
                [
                    SAssignNew(ButtonArea, SHorizontalBox)
                ]
            ]
        ]
    ];

    SetVisibility(EVisibility::Collapsed);
}

void SWowQuestDialog::ShowQuestDetails(const FWowQuestDetails& QuestDetails)
{
    CurrentQuest = QuestDetails;
    bIsRewardDialog = false;
    SelectedRewardChoice = 0;

    RefreshContent();
    SetVisibility(EVisibility::Visible);

    UE_LOG(LogWowQuestDialog, Log, TEXT("Showing quest details: %s"), *QuestDetails.Title);
}

void SWowQuestDialog::ShowQuestReward(const FWowQuestDetails& QuestDetails)
{
    CurrentQuest = QuestDetails;
    bIsRewardDialog = true;
    SelectedRewardChoice = 0;

    RefreshContent();
    SetVisibility(EVisibility::Visible);

    UE_LOG(LogWowQuestDialog, Log, TEXT("Showing quest reward: %s"), *QuestDetails.Title);
}

void SWowQuestDialog::CloseDialog()
{
    CurrentQuest = FWowQuestDetails();
    SetVisibility(EVisibility::Collapsed);

    UE_LOG(LogWowQuestDialog, Log, TEXT("Closed quest dialog"));
}

void SWowQuestDialog::RefreshContent()
{
    // Clear existing content
    ContentArea->ClearChildren();
    RewardArea->ClearChildren();
    ButtonArea->ClearChildren();

    // Quest title
    ContentArea->AddSlot()
    [
        SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
        .Padding(FMargin(8.0f))
        [
            SNew(STextBlock)
            .Text(FText::FromString(CurrentQuest.Title))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
            .ColorAndOpacity(FLinearColor::Yellow)
            .AutoWrapText(true)
        ]
    ];

    // Quest details text
    if (!CurrentQuest.Details.IsEmpty())
    {
        ContentArea->AddSlot()
        [
            SNew(STextBlock)
            .Text(FText::FromString(CurrentQuest.Details))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
            .ColorAndOpacity(FLinearColor::White)
            .AutoWrapText(true)
        ];
    }

    // Quest objectives
    if (!CurrentQuest.Objectives.IsEmpty())
    {
        ContentArea->AddSlot()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Objectives:")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
            .ColorAndOpacity(FLinearColor::Yellow)
        ];

        ContentArea->AddSlot()
        [
            SNew(STextBlock)
            .Text(FText::FromString(CurrentQuest.Objectives))
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
            .AutoWrapText(true)
        ];
    }

    // Rewards section
    if (CurrentQuest.RewardMoney > 0 || CurrentQuest.RewardXP > 0 || CurrentQuest.RewardItems.Num() > 0)
    {
        ContentArea->AddSlot()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Rewards:")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
            .ColorAndOpacity(FLinearColor::Yellow)
        ];

        // XP reward
        if (CurrentQuest.RewardXP > 0)
        {
            ContentArea->AddSlot()
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("Experience: %u XP"), CurrentQuest.RewardXP)))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                .ColorAndOpacity(FLinearColor::White)
            ];
        }

        // Money reward
        if (CurrentQuest.RewardMoney > 0)
        {
            uint32 Gold = CurrentQuest.RewardMoney / 10000;
            uint32 Silver = (CurrentQuest.RewardMoney % 10000) / 100;
            uint32 Copper = CurrentQuest.RewardMoney % 100;

            FString MoneyText;
            if (Gold > 0) MoneyText += FString::Printf(TEXT("%ug "), Gold);
            if (Silver > 0) MoneyText += FString::Printf(TEXT("%us "), Silver);
            if (Copper > 0 || CurrentQuest.RewardMoney < 100) MoneyText += FString::Printf(TEXT("%uc"), Copper);

            ContentArea->AddSlot()
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(TEXT("Money: %s"), *MoneyText.TrimEnd())))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                .ColorAndOpacity(FLinearColor::Yellow)
            ];
        }
    }

    // Choice rewards (for reward dialog)
    if (bIsRewardDialog && CurrentQuest.ChoiceRewards.Num() > 0)
    {
        ContentArea->AddSlot()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Choose your reward:")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
            .ColorAndOpacity(FLinearColor::Yellow)
        ];

        for (int32 i = 0; i < CurrentQuest.ChoiceRewards.Num(); ++i)
        {
            const FWowQuestRewardItem& RewardItem = CurrentQuest.ChoiceRewards[i];

            ContentArea->AddSlot()
            [
                SNew(SButton)
                .OnClicked_Lambda([this, i]() { return OnRewardItemClicked(i); })
                .ButtonColorAndOpacity_Lambda([this, i]() {
                    return SelectedRewardChoice == i ? FLinearColor::Blue : FLinearColor::Gray;
                })
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SBox)
                        .WidthOverride(24.0f)
                        .HeightOverride(24.0f)
                        [
                            SNew(SBorder)
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("?")))
                                .Justification(ETextJustify::Center)
                            ]
                        ]
                    ]
                    + SHorizontalBox::Slot()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("Reward Item %u (x%u)"),
                            RewardItem.ItemId, RewardItem.Count)))
                    ]
                ]
            ];
        }
    }

    // Buttons
    if (bIsRewardDialog)
    {
        // Complete Quest button
        ButtonArea->AddSlot()
        .AutoWidth()
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Complete Quest")))
            .OnClicked(this, &SWowQuestDialog::OnCompleteClicked)
        ];
    }
    else
    {
        // Accept/Decline buttons
        ButtonArea->AddSlot()
        .AutoWidth()
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Accept")))
            .OnClicked(this, &SWowQuestDialog::OnAcceptClicked)
        ];

        ButtonArea->AddSlot()
        .AutoWidth()
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("Decline")))
            .OnClicked(this, &SWowQuestDialog::OnDeclineClicked)
        ];
    }
}

FReply SWowQuestDialog::OnAcceptClicked()
{
    if (OnQuestAccept.IsBound())
    {
        OnQuestAccept.Execute(CurrentQuest.QuestGiverGuid, CurrentQuest.QuestId);
    }
    CloseDialog();
    return FReply::Handled();
}

FReply SWowQuestDialog::OnDeclineClicked()
{
    if (OnQuestDecline.IsBound())
    {
        OnQuestDecline.Execute();
    }
    CloseDialog();
    return FReply::Handled();
}

FReply SWowQuestDialog::OnCompleteClicked()
{
    if (OnQuestComplete.IsBound())
    {
        OnQuestComplete.Execute(CurrentQuest.QuestGiverGuid, CurrentQuest.QuestId, SelectedRewardChoice);
    }
    CloseDialog();
    return FReply::Handled();
}

FReply SWowQuestDialog::OnRewardItemClicked(int32 RewardIndex)
{
    SelectedRewardChoice = RewardIndex;
    // Refresh to update button colors
    RefreshContent();
    return FReply::Handled();
}