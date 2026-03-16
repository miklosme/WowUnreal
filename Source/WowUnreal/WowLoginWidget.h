#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_FourParams(FOnLoginSubmit, const FString& /*Server*/, int32 /*Port*/, const FString& /*User*/, const FString& /*Pass*/);

/** WoW expansion for login screen theming */
enum class EWowExpansion : uint8
{
    Classic = 0,
    BurningCrusade,
    WrathOfTheLichKing,
    Count
};

/**
 * WoW-themed login screen with expansion switcher.
 * Mimics the look of the original client login screens.
 */
class SWowLoginWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowLoginWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetStatusText(const FString& Text);

    FOnLoginSubmit OnLoginSubmit;

private:
    FReply OnLoginClicked();
    void SetExpansion(EWowExpansion Expansion);

    /** Build the expansion tab bar at the bottom */
    TSharedRef<SWidget> BuildExpansionTabs();

    /** Get colors for the current expansion */
    FLinearColor GetBackgroundColor() const;
    FLinearColor GetAccentColor() const;
    FString GetExpansionTitle() const;
    FString GetExpansionSubtitle() const;

    EWowExpansion CurrentExpansion = EWowExpansion::WrathOfTheLichKing;

    TSharedPtr<SEditableTextBox> ServerBox;
    TSharedPtr<SEditableTextBox> UsernameBox;
    TSharedPtr<SEditableTextBox> PasswordBox;
    TSharedPtr<STextBlock> StatusLabel;
    TSharedPtr<STextBlock> TitleText;
    TSharedPtr<STextBlock> SubtitleText;
    TSharedPtr<SBorder> BackgroundBorder;

    // Expansion tab buttons for styling updates
    TSharedPtr<SBorder> ClassicTab;
    TSharedPtr<SBorder> BCTab;
    TSharedPtr<SBorder> WotLKTab;
};
