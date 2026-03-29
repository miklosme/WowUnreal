#include "SWowMailbox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Engine/Engine.h"
#include "WowNetwork/Public/WowPacketHandler.h"
#include "WowNetwork/Public/WowOpcodes.h"

void SWowMailbox::Construct(const FArguments& InArgs, FWowPacketHandler* InPacketHandler)
{
    PacketHandler = InPacketHandler;

    // Create the main window structure
    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(350.0f)
        .HeightOverride(400.0f)
        .Visibility_Lambda([this]() { return CurrentVisibility; })
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .Padding(8.0f)
            [
                SNew(SVerticalBox)

                // Title bar with close button
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        SNew(STextBlock)
                        .Text(NSLOCTEXT("WowUI", "Mailbox", "Mailbox"))
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                        .ColorAndOpacity(FLinearColor::Yellow)
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(NSLOCTEXT("WowUI", "Close", "Close"))
                        .OnClicked(this, &SWowMailbox::OnCloseButtonClicked)
                    ]
                ]

                // Tab buttons
                + SVerticalBox::Slot()
                .AutoHeight()
                .Padding(0, 0, 0, 8)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .Padding(0, 0, 4, 0)
                    [
                        SNew(SButton)
                        .Text(NSLOCTEXT("WowUI", "Inbox", "Inbox"))
                        .OnClicked(this, &SWowMailbox::OnInboxTabClicked)
                        .ButtonColorAndOpacity_Lambda([this]() { return GetInboxTabColor(); })
                    ]

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    [
                        SNew(SButton)
                        .Text(NSLOCTEXT("WowUI", "Send", "Send"))
                        .OnClicked(this, &SWowMailbox::OnSendTabClicked)
                        .ButtonColorAndOpacity_Lambda([this]() { return GetSendTabColor(); })
                        .IsEnabled(false) // Send tab disabled for stub
                    ]
                ]

                // Content area
                + SVerticalBox::Slot()
                .FillHeight(1.0f)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text_Lambda([this]() { return GetContentText(); })
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                        .ColorAndOpacity(FLinearColor::Gray)
                        .Justification(ETextJustify::Center)
                    ]
                ]
            ]
        ]
    ];
}

void SWowMailbox::ToggleVisibility()
{
    if (CurrentVisibility == EVisibility::Visible)
    {
        CurrentVisibility = EVisibility::Hidden;
    }
    else
    {
        Show();
    }
}

void SWowMailbox::Show()
{
    CurrentVisibility = EVisibility::Visible;
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

FReply SWowMailbox::OnInboxTabClicked()
{
    ActiveTab = 0;
    return FReply::Handled();
}

FReply SWowMailbox::OnSendTabClicked()
{
    ActiveTab = 1;
    return FReply::Handled();
}

FSlateColor SWowMailbox::GetInboxTabColor() const
{
    return (ActiveTab == 0) ? FLinearColor::Yellow : FLinearColor::White;
}

FSlateColor SWowMailbox::GetSendTabColor() const
{
    return (ActiveTab == 1) ? FLinearColor::Yellow : FLinearColor::White;
}

FText SWowMailbox::GetContentText() const
{
    if (ActiveTab == 0)
    {
        return NSLOCTEXT("WowUI", "NoMail", "No mail");
    }
    else
    {
        return NSLOCTEXT("WowUI", "SendNotImplemented", "Send mail not implemented yet");
    }
}

void SWowMailbox::RequestMailList()
{
    if (PacketHandler && PacketHandler->OnSendPacket.IsBound())
    {
        TArray<uint8> EmptyData;
        PacketHandler->OnSendPacket.ExecuteIfBound(WowOpcode::CMSG_GET_MAIL_LIST, EmptyData);
    }
}