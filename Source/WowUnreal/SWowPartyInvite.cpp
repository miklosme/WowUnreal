#include "SWowPartyInvite.h"
#include "WowConnectionManager.h"
#include "WowOpcodes.h"
#include "Widgets/Layout/SVerticalBox.h"
#include "Widgets/Layout/SHorizontalBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowPartyInvite, Log, All);

void SWowPartyInvite::Construct(const FArguments& InArgs)
{
    ConnectionManager = InArgs._ConnectionManager;
    InviterName = InArgs._InviterName;

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .BorderBackgroundColor(FLinearColor(0.05f, 0.05f, 0.05f, 0.95f))
        .Padding(16.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 0, 0, 12)
            [
                SNew(STextBlock)
                .Text(this, &SWowPartyInvite::GetInviteMessageText)
                .ColorAndOpacity(FLinearColor::White)
                .Font(FAppStyle::GetFontStyle("NormalFont"))
                .Justification(ETextJustify::Center)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(0, 0, 4, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Accept")))
                    .OnClicked(this, &SWowPartyInvite::OnAcceptClicked)
                    .ButtonColorAndOpacity(FLinearColor(0.0f, 0.6f, 0.0f))
                ]
                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(4, 0, 0, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Decline")))
                    .OnClicked(this, &SWowPartyInvite::OnDeclineClicked)
                    .ButtonColorAndOpacity(FLinearColor(0.6f, 0.0f, 0.0f))
                ]
            ]
        ]
    ];
}

void SWowPartyInvite::CloseDialog()
{
    // Remove this widget from the parent by invalidating itself
    SetVisibility(EVisibility::Collapsed);
}

FReply SWowPartyInvite::OnAcceptClicked()
{
    UE_LOG(LogWowPartyInvite, Log, TEXT("Accepting party invite from %s"), *InviterName);

    if (ConnectionManager.IsValid())
    {
        // Send CMSG_GROUP_ACCEPT with no payload
        TArray<uint8> Data;
        ConnectionManager->SendPacket(WowOpcode::CMSG_GROUP_ACCEPT, Data);
    }

    CloseDialog();
    return FReply::Handled();
}

FReply SWowPartyInvite::OnDeclineClicked()
{
    UE_LOG(LogWowPartyInvite, Log, TEXT("Declining party invite from %s"), *InviterName);

    if (ConnectionManager.IsValid())
    {
        // Send CMSG_GROUP_DECLINE with no payload
        TArray<uint8> Data;
        ConnectionManager->SendPacket(WowOpcode::CMSG_GROUP_DECLINE, Data);
    }

    CloseDialog();
    return FReply::Handled();
}

FText SWowPartyInvite::GetInviteMessageText() const
{
    return FText::FromString(FString::Printf(TEXT("%s has invited you to a group."), *InviterName));
}