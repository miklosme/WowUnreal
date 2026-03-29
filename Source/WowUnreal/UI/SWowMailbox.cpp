#include "SWowMailbox.h"
#include "WowNetwork/Public/WowConnectionManager.h"
#include "WowNetwork/Public/WowPacketHandler.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "WowMailbox"

namespace
{
FString BuildAttachmentSummary(const FWowMailMessage& Mail)
{
    if (Mail.Attachments.IsEmpty())
    {
        return TEXT("");
    }

    TArray<FString> AttachmentParts;
    AttachmentParts.Reserve(Mail.Attachments.Num());

    for (const FWowMailAttachment& Attachment : Mail.Attachments)
    {
        AttachmentParts.Add(FString::Printf(TEXT("Item %u x%u"), Attachment.ItemEntry, Attachment.Count));
    }

    return FString::Join(AttachmentParts, TEXT(", "));
}
}

void SWowMailbox::Construct(const FArguments& InArgs, UWowConnectionManager* InConnectionManager)
{
    ConnectionManager = InConnectionManager;

    if (ConnectionManager.IsValid())
    {
        ConnectionManager->PacketHandler.OnMailListReceived.AddSP(this, &SWowMailbox::OnMailListReceived);
    }

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(430.0f)
        .HeightOverride(480.0f)
        .Visibility_Lambda([this]() { return CurrentVisibility; })
        [
            SNew(SBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
            .Padding(8.0f)
            [
                SNew(SVerticalBox)

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("MailboxTitle", "Mailbox"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                        .ColorAndOpacity(FLinearColor::Yellow)
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 6, 0)
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("RefreshMail", "Refresh"))
                        .OnClicked(this, &SWowMailbox::OnRefreshButtonClicked)
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(LOCTEXT("CloseMailbox", "Close"))
                        .OnClicked(this, &SWowMailbox::OnCloseButtonClicked)
                    ]
                ]

                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    SNew(STextBlock)
                    .Text_Lambda([this]()
                    {
                        if (bWaitingForMailList)
                        {
                            return LOCTEXT("MailboxLoading", "Loading mailbox...");
                        }

                        if (CurrentMailboxGuid == 0)
                        {
                            return LOCTEXT("MailboxClosedStatus", "No mailbox selected.");
                        }

                        return FText::FromString(FString::Printf(TEXT("%d mails in inbox"), CurrentMail.Num()));
                    })
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                    .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
                ]

                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SAssignNew(MailList, SScrollBox)
                    .Orientation(Orient_Vertical)
                ]
            ]
        ]
    ];

    RefreshInboxList();
}

void SWowMailbox::ToggleVisibility()
{
    CurrentVisibility = (CurrentVisibility == EVisibility::Visible) ? EVisibility::Hidden : EVisibility::Visible;
}

void SWowMailbox::Show(uint64 MailboxGuid)
{
    CurrentMailboxGuid = MailboxGuid;
    CurrentVisibility = EVisibility::Visible;
    bWaitingForMailList = true;
    CurrentMail.Reset();
    RefreshInboxList();
    RequestMailList();
}

bool SWowMailbox::IsVisible() const
{
    return CurrentVisibility == EVisibility::Visible;
}

FReply SWowMailbox::OnCloseButtonClicked()
{
    CurrentVisibility = EVisibility::Hidden;
    return FReply::Handled();
}

FReply SWowMailbox::OnRefreshButtonClicked()
{
    bWaitingForMailList = true;
    CurrentMail.Reset();
    RefreshInboxList();
    RequestMailList();
    return FReply::Handled();
}

void SWowMailbox::RequestMailList()
{
    UWowConnectionManager* Connection = ConnectionManager.Get();
    if (!Connection || CurrentMailboxGuid == 0)
    {
        bWaitingForMailList = false;
        RefreshInboxList();
        return;
    }

    Connection->SendGetMailList(static_cast<int64>(CurrentMailboxGuid));
}

void SWowMailbox::OnMailListReceived(const TArray<FWowMailMessage>& InMail)
{
    CurrentMail = InMail;
    bWaitingForMailList = false;
    RefreshInboxList();
}

void SWowMailbox::RefreshInboxList()
{
    if (!MailList.IsValid())
    {
        return;
    }

    MailList->ClearChildren();

    if (bWaitingForMailList)
    {
        MailList->AddSlot()
        [
            SNew(STextBlock)
            .Text(LOCTEXT("MailboxLoadingBody", "Requesting inbox contents from the server..."))
            .ColorAndOpacity(FLinearColor::Gray)
        ];
        return;
    }

    if (CurrentMail.IsEmpty())
    {
        MailList->AddSlot()
        [
            SNew(STextBlock)
            .Text(LOCTEXT("MailboxEmpty", "No mail."))
            .ColorAndOpacity(FLinearColor::Gray)
        ];
        return;
    }

    for (const FWowMailMessage& Mail : CurrentMail)
    {
        MailList->AddSlot()
        [
            BuildMailRow(Mail)
        ];
    }
}

TSharedRef<SWidget> SWowMailbox::BuildMailRow(const FWowMailMessage& Mail) const
{
    const bool bIsRead = (Mail.Checked & WowMailCheckMask::READ) != 0;
    const FString AttachmentSummary = BuildAttachmentSummary(Mail);
    const FString SenderLine = FString::Printf(TEXT("From: %s | %.1f days left"), *GetSenderLabel(Mail), Mail.DaysLeft);
    const FString MoneyLine = FString::Printf(TEXT("Money: %s | C.O.D.: %s"),
        *GetMoneyText(Mail.Money).ToString(),
        *GetMoneyText(Mail.COD).ToString());

    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
        .Padding(8.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(Mail.Subject.IsEmpty() ? TEXT("(No subject)") : Mail.Subject))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
                .ColorAndOpacity(bIsRead ? FLinearColor::White : FLinearColor::Yellow)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 2, 0, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(SenderLine))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 2, 0, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(MoneyLine))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.3f))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 2, 0, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(AttachmentSummary.IsEmpty() ? TEXT("Attachments: none") : FString::Printf(TEXT("Attachments: %s"), *AttachmentSummary)))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0, 4, 0, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Mail.Body.IsEmpty() ? TEXT("(No body)") : Mail.Body))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                .ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f))
                .AutoWrapText(true)
            ]
        ];
}

FString SWowMailbox::GetSenderLabel(const FWowMailMessage& Mail) const
{
    const UWowConnectionManager* Connection = ConnectionManager.Get();

    switch (Mail.MessageType)
    {
    case WowMailMessageType::NORMAL:
        if (Connection)
        {
            if (const FString* PlayerName = Connection->PacketHandler.PlayerNameCache.Find(Mail.SenderGuid))
            {
                return *PlayerName;
            }
        }
        return Mail.SenderGuid != 0
            ? FString::Printf(TEXT("Player %llu"), Mail.SenderGuid)
            : TEXT("Player");

    case WowMailMessageType::CREATURE:
        if (Connection)
        {
            if (const FString* CreatureName = Connection->PacketHandler.CreatureNameCache.Find(Mail.SenderEntry))
            {
                return *CreatureName;
            }
        }
        return FString::Printf(TEXT("Creature %u"), Mail.SenderEntry);

    case WowMailMessageType::GAMEOBJECT:
        return FString::Printf(TEXT("GameObject %u"), Mail.SenderEntry);

    case WowMailMessageType::AUCTION:
        return TEXT("Auction House");

    case WowMailMessageType::CALENDAR:
        return TEXT("Calendar");

    default:
        return TEXT("Unknown sender");
    }
}

FText SWowMailbox::GetMoneyText(uint32 Copper) const
{
    const uint32 Gold = Copper / 10000;
    const uint32 Silver = (Copper / 100) % 100;
    const uint32 CopperOnly = Copper % 100;
    return FText::FromString(FString::Printf(TEXT("%ug %us %uc"), Gold, Silver, CopperOnly));
}

#undef LOCTEXT_NAMESPACE
