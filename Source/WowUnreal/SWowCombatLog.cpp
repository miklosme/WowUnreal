#include "SWowCombatLog.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"

void SWowCombatLog::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SBox)
        .WidthOverride(400.0f)
        .HeightOverride(200.0f)
        [
            SNew(SBorder)
            .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f)) // Semi-transparent dark background
            .BorderImage(FCoreStyle::Get().GetBrush("Border"))
            .Padding(FMargin(5.0f))
            [
                SAssignNew(ScrollBox, SScrollBox)
                .Orientation(EOrientation::Orient_Vertical)
                .ScrollBarAlwaysVisible(false)
                .AllowOverscroll(EAllowOverscroll::No)
                + SScrollBox::Slot()
                [
                    SAssignNew(MessageContainer, SVerticalBox)
                ]
            ]
        ]
    ];
}

void SWowCombatLog::AddCombatMessage(const FString& Message, const FLinearColor& Color)
{
    if (!MessageContainer.IsValid())
    {
        return;
    }

    // Create a new text block for this message
    TSharedRef<STextBlock> MessageText = SNew(STextBlock)
        .Text(FText::FromString(Message))
        .ColorAndOpacity(Color)
        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
        .AutoWrapText(true);

    // Add to container
    MessageContainer->AddSlot()
        .AutoHeight()
        .Padding(FMargin(2.0f, 1.0f))
        [
            MessageText
        ];

    MessageCount++;

    // Remove old messages if we exceed the limit
    if (MessageCount > MaxMessages)
    {
        MessageContainer->RemoveSlot(MessageContainer->GetChildren()->GetChildAt(0));
        MessageCount--;
    }

    // Auto-scroll to bottom
    ScrollToBottom();
}

void SWowCombatLog::ClearMessages()
{
    if (MessageContainer.IsValid())
    {
        MessageContainer->ClearChildren();
        MessageCount = 0;
    }
}

void SWowCombatLog::ScrollToBottom()
{
    if (ScrollBox.IsValid())
    {
        ScrollBox->ScrollToEnd();
    }
}