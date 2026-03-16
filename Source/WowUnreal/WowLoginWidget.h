#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "WowExpansionTypes.h"

DECLARE_DELEGATE_FourParams(FOnLoginSubmit, const FString& /*Server*/, int32 /*Port*/, const FString& /*User*/, const FString& /*Pass*/);
DECLARE_DELEGATE_OneParam(FOnExpansionChanged, uint8 /*ExpansionIndex*/);

class UMediaTexture;
struct FSlateBrush;

/**
 * WoW-themed login screen with expansion switcher.
 * Mimics the look of the original client login screens.
 * Supports cinematic video background via media texture.
 */
class SWowLoginWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowLoginWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetStatusText(const FString& Text);

    /** Prefill the login fields */
    void SetCredentials(const FString& Server, const FString& Username, const FString& Password);

    /** Programmatically click the login button */
    void AutoSubmit();

    /** Set the media texture for cinematic background */
    void SetCinematicTexture(UMediaTexture* InTexture);

    FOnLoginSubmit OnLoginSubmit;
    FOnExpansionChanged OnExpansionChanged;

    EWowExpansion GetCurrentExpansion() const { return CurrentExpansion; }

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

    // Cinematic background
    TSharedPtr<SImage> CinematicImage;
    TSharedPtr<FSlateBrush> CinematicBrush;

    // Expansion tab buttons for styling updates
    TSharedPtr<SBorder> ClassicTab;
    TSharedPtr<SBorder> BCTab;
    TSharedPtr<SBorder> WotLKTab;
};
