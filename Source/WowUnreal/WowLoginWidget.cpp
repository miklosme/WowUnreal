#include "WowLoginWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"

void SWowLoginWidget::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.05f, 0.85f))
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        [
        SNew(SVerticalBox)

        // Spacer to center vertically
        + SVerticalBox::Slot().FillHeight(1.0f)

        // Title
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 30)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("World of Warcraft Login")))
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 28))
            .ColorAndOpacity(FLinearColor(1.0f, 0.84f, 0.0f))
        ]

        // Server field
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Server:")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).MinDesiredWidth(300)
                [
                    SAssignNew(ServerBox, SEditableTextBox)
                    .Text(FText::FromString(TEXT("192.168.1.5:3724")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                ]
            ]
        ]

        // Username field
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Username:")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).MinDesiredWidth(300)
                [
                    SAssignNew(UsernameBox, SEditableTextBox)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                ]
            ]
        ]

        // Password field
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("Password:")))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                .ColorAndOpacity(FLinearColor::White)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).MinDesiredWidth(300)
                [
                    SAssignNew(PasswordBox, SEditableTextBox)
                    .IsPassword(true)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                ]
            ]
        ]

        // Login button
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 20, 0, 10)
        [
            SNew(SButton)
            .OnClicked_Raw(this, &SWowLoginWidget::OnLoginClicked)
            [
                SNew(SBox).Padding(FMargin(20, 5))
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Login")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
                    .Justification(ETextJustify::Center)
                ]
            ]
        ]

        // Status label
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 5)
        [
            SAssignNew(StatusLabel, STextBlock)
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
            .ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
        ]

        // Bottom spacer
        + SVerticalBox::Slot().FillHeight(1.0f)
        ] // SBorder
    ];
}

FReply SWowLoginWidget::OnLoginClicked()
{
    FString ServerStr = ServerBox.IsValid() ? ServerBox->GetText().ToString().TrimStartAndEnd() : TEXT("192.168.1.5:3724");
    FString User = UsernameBox.IsValid() ? UsernameBox->GetText().ToString().TrimStartAndEnd() : TEXT("");
    FString Pass = PasswordBox.IsValid() ? PasswordBox->GetText().ToString().TrimStartAndEnd() : TEXT("");

    FString Host = ServerStr;
    int32 Port = 3724;
    int32 ColonIdx;
    if (ServerStr.FindChar(TEXT(':'), ColonIdx))
    {
        Host = ServerStr.Left(ColonIdx);
        Port = FCString::Atoi(*ServerStr.Mid(ColonIdx + 1));
    }

    if (User.IsEmpty())
    {
        SetStatusText(TEXT("Please enter a username"));
        return FReply::Handled();
    }

    SetStatusText(TEXT("Connecting..."));
    OnLoginSubmit.ExecuteIfBound(Host, Port, User, Pass);
    return FReply::Handled();
}

void SWowLoginWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel.IsValid())
    {
        StatusLabel->SetText(FText::FromString(Text));
    }
}
