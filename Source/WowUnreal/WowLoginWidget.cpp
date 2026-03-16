#include "WowLoginWidget.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "MediaTexture.h"

// ─── Expansion-specific theming ─────────────────────────────────────────────

FLinearColor SWowLoginWidget::GetBackgroundColor() const
{
    switch (CurrentExpansion)
    {
    case EWowExpansion::Classic:
        return FLinearColor(0.02f, 0.02f, 0.08f, 0.95f); // Deep dark blue
    case EWowExpansion::BurningCrusade:
        return FLinearColor(0.06f, 0.02f, 0.02f, 0.95f); // Dark red/fel green tint
    case EWowExpansion::WrathOfTheLichKing:
    default:
        return FLinearColor(0.02f, 0.04f, 0.08f, 0.95f); // Icy dark blue
    }
}

FLinearColor SWowLoginWidget::GetAccentColor() const
{
    switch (CurrentExpansion)
    {
    case EWowExpansion::Classic:
        return FLinearColor(1.0f, 0.84f, 0.0f); // Classic gold
    case EWowExpansion::BurningCrusade:
        return FLinearColor(0.4f, 1.0f, 0.2f); // Fel green
    case EWowExpansion::WrathOfTheLichKing:
    default:
        return FLinearColor(0.6f, 0.85f, 1.0f); // Icy blue
    }
}

FString SWowLoginWidget::GetExpansionTitle() const
{
    switch (CurrentExpansion)
    {
    case EWowExpansion::Classic:
        return TEXT("World of Warcraft");
    case EWowExpansion::BurningCrusade:
        return TEXT("The Burning Crusade");
    case EWowExpansion::WrathOfTheLichKing:
    default:
        return TEXT("Wrath of the Lich King");
    }
}

FString SWowLoginWidget::GetExpansionSubtitle() const
{
    switch (CurrentExpansion)
    {
    case EWowExpansion::Classic:
        return TEXT("");
    case EWowExpansion::BurningCrusade:
        return TEXT("World of Warcraft");
    case EWowExpansion::WrathOfTheLichKing:
    default:
        return TEXT("World of Warcraft");
    }
}

// ─── UI Construction ────────────────────────────────────────────────────────

void SWowLoginWidget::Construct(const FArguments& InArgs)
{
    // WoW gold color palette
    const FLinearColor WowGold(1.0f, 0.84f, 0.0f);
    const FLinearColor WowGoldDim(0.7f, 0.6f, 0.1f);
    const FLinearColor FieldBg(0.0f, 0.0f, 0.0f, 0.6f);
    const FLinearColor FieldBorder(0.35f, 0.3f, 0.1f, 0.8f);
    const FLinearColor PanelBorder(0.4f, 0.35f, 0.1f, 0.6f);

    // Styled text field builder
    auto MakeField = [&](TSharedPtr<SEditableTextBox>& OutBox, const FString& HintText, bool bPassword = false) -> TSharedRef<SWidget>
    {
        return SNew(SBorder)
            .BorderBackgroundColor(FieldBg)
            .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
            .Padding(FMargin(2))
            [
                SNew(SBorder)
                .BorderBackgroundColor(FieldBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                .Padding(FMargin(1))
                [
                    SNew(SBox).MinDesiredWidth(280).MaxDesiredHeight(30)
                    [
                        SAssignNew(OutBox, SEditableTextBox)
                        .IsPassword(bPassword)
                        .HintText(FText::FromString(HintText))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                        .BackgroundColor(FLinearColor(0.05f, 0.05f, 0.1f, 1.0f))
                        .ForegroundColor(FSlateColor(FLinearColor::White))
                    ]
                ]
            ];
    };

    ChildSlot
    [
        SNew(SOverlay)

        // Layer 0: Cinematic video background (hidden until texture is set)
        + SOverlay::Slot()
        [
            SAssignNew(CinematicImage, SImage)
            .Visibility(EVisibility::Collapsed)
        ]

        // Layer 1: Color overlay + UI
        + SOverlay::Slot()
        [
        SAssignNew(BackgroundBorder, SBorder)
        .BorderBackgroundColor(GetBackgroundColor())
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        [
            SNew(SVerticalBox)

            // ═══ Top spacer ═══
            + SVerticalBox::Slot().FillHeight(1.5f)

            // ═══ Title area ═══
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 5)
            [
                SAssignNew(SubtitleText, STextBlock)
                .Text(FText::FromString(GetExpansionSubtitle()))
                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
                .ColorAndOpacity(FLinearColor(0.8f, 0.75f, 0.5f))
                .Justification(ETextJustify::Center)
            ]

            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 0, 0, 30)
            [
                SAssignNew(TitleText, STextBlock)
                .Text(FText::FromString(GetExpansionTitle()))
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 36))
                .ColorAndOpacity(GetAccentColor())
                .Justification(ETextJustify::Center)
            ]

            // ═══ Login panel (centered, bordered) ═══
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.4f))
                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                .Padding(FMargin(30, 25))
                [
                    SNew(SBorder)
                    .BorderBackgroundColor(PanelBorder)
                    .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                    .Padding(FMargin(1))
                    [
                        SNew(SBorder)
                        .BorderBackgroundColor(FLinearColor(0.03f, 0.03f, 0.06f, 0.95f))
                        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                        .Padding(FMargin(25, 20))
                        [
                            SNew(SVerticalBox)

                            // Server field
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Server")))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                                    .ColorAndOpacity(WowGoldDim)
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    MakeField(ServerBox, TEXT("127.0.0.1:3724"))
                                ]
                            ]

                            // Username field
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 4)
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Account Name")))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                                    .ColorAndOpacity(WowGoldDim)
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    MakeField(UsernameBox, TEXT(""))
                                ]
                            ]

                            // Password field
                            + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 4)
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 3)
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Password")))
                                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                                    .ColorAndOpacity(WowGoldDim)
                                ]
                                + SVerticalBox::Slot().AutoHeight()
                                [
                                    MakeField(PasswordBox, TEXT(""), true)
                                ]
                            ]

                            // Login button
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 18, 0, 8)
                            [
                                SNew(SBorder)
                                .BorderBackgroundColor(FLinearColor(0.35f, 0.28f, 0.05f, 1.0f))
                                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                .Padding(FMargin(1))
                                [
                                    SNew(SButton)
                                    .ButtonColorAndOpacity(FLinearColor(0.15f, 0.12f, 0.02f, 1.0f))
                                    .OnClicked_Raw(this, &SWowLoginWidget::OnLoginClicked)
                                    [
                                        SNew(SBox).Padding(FMargin(40, 6)).HAlign(HAlign_Center)
                                        [
                                            SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("Login")))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                                            .ColorAndOpacity(WowGold)
                                            .Justification(ETextJustify::Center)
                                        ]
                                    ]
                                ]
                            ]

                            // Status label
                            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 4)
                            [
                                SAssignNew(StatusLabel, STextBlock)
                                .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                                .ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.3f))
                            ]
                        ]
                    ]
                ]
            ]

            // ═══ Bottom spacer ═══
            + SVerticalBox::Slot().FillHeight(2.0f)

            // ═══ Expansion tabs ═══
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 0)
            [
                BuildExpansionTabs()
            ]
        ]
        ] // SOverlay Layer 1
    ];
}

TSharedRef<SWidget> SWowLoginWidget::BuildExpansionTabs()
{
    const FLinearColor TabBarBg(0.0f, 0.0f, 0.0f, 0.7f);
    const FLinearColor TabBorderColor(0.3f, 0.25f, 0.08f, 0.8f);
    const FLinearColor InactiveTab(0.06f, 0.05f, 0.02f, 0.9f);
    const FLinearColor ActiveClassic(0.12f, 0.1f, 0.03f, 0.95f);
    const FLinearColor ActiveBC(0.05f, 0.12f, 0.03f, 0.95f);
    const FLinearColor ActiveWotLK(0.03f, 0.06f, 0.12f, 0.95f);

    auto MakeTab = [&](const FString& Label, EWowExpansion Exp, TSharedPtr<SBorder>& OutBorder) -> TSharedRef<SWidget>
    {
        bool bActive = (CurrentExpansion == Exp);
        FLinearColor ActiveColor;
        switch (Exp)
        {
        case EWowExpansion::Classic: ActiveColor = ActiveClassic; break;
        case EWowExpansion::BurningCrusade: ActiveColor = ActiveBC; break;
        default: ActiveColor = ActiveWotLK; break;
        }

        return SNew(SBox).Padding(FMargin(2, 0))
        [
            SAssignNew(OutBorder, SBorder)
            .BorderBackgroundColor(TabBorderColor)
            .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
            .Padding(FMargin(1))
            [
                SNew(SButton)
                .ButtonColorAndOpacity(bActive ? ActiveColor : InactiveTab)
                .OnClicked_Lambda([this, Exp]() -> FReply
                {
                    SetExpansion(Exp);
                    return FReply::Handled();
                })
                [
                    SNew(SBox).Padding(FMargin(20, 8)).HAlign(HAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Label))
                        .Font(FCoreStyle::GetDefaultFontStyle(bActive ? "Bold" : "Regular", 13))
                        .ColorAndOpacity(bActive ? GetAccentColor() : FLinearColor(0.5f, 0.45f, 0.3f))
                    ]
                ]
            ]
        ];
    };

    return SNew(SBorder)
        .BorderBackgroundColor(TabBarBg)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        .Padding(FMargin(0, 4))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            + SHorizontalBox::Slot().AutoWidth()
            [
                MakeTab(TEXT("Classic"), EWowExpansion::Classic, ClassicTab)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                MakeTab(TEXT("The Burning Crusade"), EWowExpansion::BurningCrusade, BCTab)
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                MakeTab(TEXT("Wrath of the Lich King"), EWowExpansion::WrathOfTheLichKing, WotLKTab)
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f)
        ];
}

void SWowLoginWidget::SetCinematicTexture(UMediaTexture* InTexture)
{
    if (!CinematicImage.IsValid()) return;

    if (InTexture)
    {
        // Create a brush from the media texture
        CinematicBrush = MakeShared<FSlateBrush>();
        CinematicBrush->SetResourceObject(InTexture);
        CinematicBrush->ImageSize = FVector2D(1920, 1080);
        CinematicBrush->DrawAs = ESlateBrushDrawType::Image;
        CinematicBrush->Tiling = ESlateBrushTileType::NoTile;

        CinematicImage->SetImage(CinematicBrush.Get());
        CinematicImage->SetVisibility(EVisibility::SelfHitTestInvisible);

        // Make the color overlay semi-transparent so cinematic shows through
        if (BackgroundBorder.IsValid())
        {
            FLinearColor BgColor = GetBackgroundColor();
            BgColor.A = 0.3f; // Let video show through
            BackgroundBorder->SetBorderBackgroundColor(BgColor);
        }
    }
    else
    {
        CinematicImage->SetVisibility(EVisibility::Collapsed);
        CinematicBrush.Reset();

        // Restore opaque background
        if (BackgroundBorder.IsValid())
        {
            BackgroundBorder->SetBorderBackgroundColor(GetBackgroundColor());
        }
    }
}

void SWowLoginWidget::SetExpansion(EWowExpansion Expansion)
{
    CurrentExpansion = Expansion;

    // Update visuals
    if (BackgroundBorder.IsValid())
    {
        // If cinematic is playing, keep semi-transparent; otherwise opaque
        FLinearColor BgColor = GetBackgroundColor();
        if (CinematicImage.IsValid() && CinematicImage->GetVisibility() != EVisibility::Collapsed)
        {
            BgColor.A = 0.3f;
        }
        BackgroundBorder->SetBorderBackgroundColor(BgColor);
    }
    if (TitleText.IsValid())
    {
        TitleText->SetText(FText::FromString(GetExpansionTitle()));
        TitleText->SetColorAndOpacity(GetAccentColor());
    }
    if (SubtitleText.IsValid())
    {
        SubtitleText->SetText(FText::FromString(GetExpansionSubtitle()));
    }

    // Notify controller to switch cinematic
    OnExpansionChanged.ExecuteIfBound(static_cast<uint8>(Expansion));
}

// ─── Login logic ────────────────────────────────────────────────────────────

FReply SWowLoginWidget::OnLoginClicked()
{
    FString ServerStr = ServerBox.IsValid() ? ServerBox->GetText().ToString().TrimStartAndEnd() : TEXT("127.0.0.1:3724");
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
        SetStatusText(TEXT("Please enter your account name"));
        return FReply::Handled();
    }

    SetStatusText(TEXT("Connecting..."));
    OnLoginSubmit.ExecuteIfBound(Host, Port, User, Pass);
    return FReply::Handled();
}

void SWowLoginWidget::SetCredentials(const FString& Server, const FString& Username, const FString& Password)
{
    if (ServerBox.IsValid()) ServerBox->SetText(FText::FromString(Server));
    if (UsernameBox.IsValid()) UsernameBox->SetText(FText::FromString(Username));
    if (PasswordBox.IsValid()) PasswordBox->SetText(FText::FromString(Password));
}

void SWowLoginWidget::AutoSubmit()
{
    OnLoginClicked();
}

void SWowLoginWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel.IsValid())
    {
        StatusLabel->SetText(FText::FromString(Text));
    }
}
