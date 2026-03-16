#include "WowCharacterSelectWidget.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Engine/TextureRenderTarget2D.h"

static FString GetRaceName(uint8 Race)
{
    switch (Race)
    {
    case 1: return TEXT("Human"); case 2: return TEXT("Orc"); case 3: return TEXT("Dwarf");
    case 4: return TEXT("Night Elf"); case 5: return TEXT("Undead"); case 6: return TEXT("Tauren");
    case 7: return TEXT("Gnome"); case 8: return TEXT("Troll"); case 10: return TEXT("Blood Elf");
    case 11: return TEXT("Draenei"); default: return FString::Printf(TEXT("Race%d"), Race);
    }
}

static FString GetClassName(uint8 Class)
{
    switch (Class)
    {
    case 1: return TEXT("Warrior"); case 2: return TEXT("Paladin"); case 3: return TEXT("Hunter");
    case 4: return TEXT("Rogue"); case 5: return TEXT("Priest"); case 6: return TEXT("Death Knight");
    case 7: return TEXT("Shaman"); case 8: return TEXT("Mage"); case 9: return TEXT("Warlock");
    case 11: return TEXT("Druid"); default: return FString::Printf(TEXT("Class%d"), Class);
    }
}

// Get class color (matching WoW class colors)
static FLinearColor GetClassColor(uint8 Class)
{
    switch (Class)
    {
    case 1: return FLinearColor(0.78f, 0.61f, 0.43f); // Warrior - tan
    case 2: return FLinearColor(0.96f, 0.55f, 0.73f); // Paladin - pink
    case 3: return FLinearColor(0.67f, 0.83f, 0.45f); // Hunter - green
    case 4: return FLinearColor(1.0f, 0.96f, 0.41f);  // Rogue - yellow
    case 5: return FLinearColor(1.0f, 1.0f, 1.0f);    // Priest - white
    case 6: return FLinearColor(0.77f, 0.12f, 0.23f);  // DK - red
    case 7: return FLinearColor(0.0f, 0.44f, 0.87f);  // Shaman - blue
    case 8: return FLinearColor(0.41f, 0.80f, 0.94f);  // Mage - light blue
    case 9: return FLinearColor(0.58f, 0.51f, 0.79f);  // Warlock - purple
    case 11: return FLinearColor(1.0f, 0.49f, 0.04f);  // Druid - orange
    default: return FLinearColor(0.8f, 0.8f, 0.8f);
    }
}

// WoW-style shared colors
namespace WowCSUI
{
    const FLinearColor Gold(1.0f, 0.84f, 0.0f);
    const FLinearColor GoldDim(0.7f, 0.6f, 0.1f);
    const FLinearColor Background(0.02f, 0.04f, 0.08f, 0.95f);
    const FLinearColor PanelBg(0.03f, 0.03f, 0.06f, 0.95f);
    const FLinearColor PanelBorder(0.4f, 0.35f, 0.1f, 0.6f);
    const FLinearColor ButtonBg(0.15f, 0.12f, 0.02f, 1.0f);
    const FLinearColor ButtonBorder(0.35f, 0.28f, 0.05f, 1.0f);
    const FLinearColor ListItem(0.05f, 0.05f, 0.08f, 0.9f);
}

void SWowCharacterSelectWidget::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SBorder)
        .BorderBackgroundColor(WowCSUI::Background)
        .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
        [
            SNew(SHorizontalBox)

            // ═══ Left panel: Character list ═══
            + SHorizontalBox::Slot().AutoWidth().Padding(30, 30, 15, 30)
            [
                SNew(SBorder)
                .BorderBackgroundColor(WowCSUI::PanelBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                .Padding(FMargin(1))
                [
                    SNew(SBorder)
                    .BorderBackgroundColor(WowCSUI::PanelBg)
                    .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                    .Padding(FMargin(15, 12))
                    [
                        SNew(SVerticalBox)

                        // Title
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Characters")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
                            .ColorAndOpacity(WowCSUI::Gold)
                        ]

                        // Character list
                        + SVerticalBox::Slot().FillHeight(1.0f)
                        [
                            SNew(SBox).MinDesiredWidth(320).MaxDesiredHeight(400)
                            [
                                SNew(SScrollBox)
                                + SScrollBox::Slot()
                                [
                                    SAssignNew(CharListBox, SVerticalBox)
                                ]
                            ]
                        ]

                        // Buttons
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
                            [
                                SNew(SBorder)
                                .BorderBackgroundColor(WowCSUI::ButtonBorder)
                                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                                .Padding(FMargin(1))
                                [
                                    SNew(SButton)
                                    .ButtonColorAndOpacity(WowCSUI::ButtonBg)
                                    .OnClicked_Lambda([this]() -> FReply
                                    {
                                        OnCreateCharacterRequest.ExecuteIfBound();
                                        return FReply::Handled();
                                    })
                                    [
                                        SNew(SBox).Padding(FMargin(15, 5))
                                        [
                                            SNew(STextBlock)
                                            .Text(FText::FromString(TEXT("Create New")))
                                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
                                            .ColorAndOpacity(WowCSUI::Gold)
                                        ]
                                    ]
                                ]
                            ]
                        ]

                        // Status
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                        [
                            SAssignNew(StatusLabel, STextBlock)
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
                            .ColorAndOpacity(FLinearColor(0.9f, 0.7f, 0.3f))
                        ]
                    ]
                ]
            ]

            // ═══ Right side: 3D character preview ═══
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(15, 30, 30, 30)
            [
                SNew(SBorder)
                .BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.3f))
                .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
                [
                    SNew(SOverlay)

                    // Preview image (hidden until render target is set)
                    + SOverlay::Slot()
                    .HAlign(HAlign_Fill)
                    .VAlign(VAlign_Fill)
                    [
                        SAssignNew(PreviewImage, SImage)
                        .Visibility(EVisibility::Collapsed)
                    ]

                    // Placeholder text (shown when no preview)
                    + SOverlay::Slot()
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    [
                        SAssignNew(PreviewPlaceholder, STextBlock)
                        .Text(FText::FromString(TEXT("Select a character")))
                        .Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
                        .ColorAndOpacity(FLinearColor(0.3f, 0.3f, 0.4f))
                    ]
                ]
            ]
        ]
    ];
}

void SWowCharacterSelectWidget::PopulateCharacters(const TArray<FWowCharacterInfo>& Characters)
{
    if (!CharListBox.IsValid()) return;
    CharListBox->ClearChildren();
    CachedCharacters = Characters;

    if (Characters.Num() == 0)
    {
        SetStatusText(TEXT("No characters found — create a new one!"));
        return;
    }

    for (int32 i = 0; i < Characters.Num(); i++)
    {
        const FWowCharacterInfo& C = Characters[i];
        FLinearColor ClassColor = GetClassColor(C.Class);
        int64 CharGuid = C.Guid;

        // Character info lines
        FString NameLine = C.Name;
        FString DetailLine = FString::Printf(TEXT("Level %d %s %s"),
            C.Level, *GetRaceName(C.Race), *GetClassName(C.Class));

        uint8 CharRace = C.Race;
        uint8 CharGender = C.Gender;

        CharListBox->AddSlot().AutoHeight().Padding(0, 2)
        [
            SNew(SBorder)
            .BorderBackgroundColor(WowCSUI::ButtonBorder)
            .BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
            .Padding(FMargin(1))
            [
                SNew(SButton)
                .ButtonColorAndOpacity(WowCSUI::ListItem)
                .OnClicked_Lambda([this, CharGuid, CharRace, CharGender]() -> FReply
                {
                    // First click highlights (shows preview), second click enters world
                    OnCharacterHighlighted.ExecuteIfBound(CharRace, CharGender);
                    OnCharacterSelected.ExecuteIfBound(CharGuid);
                    return FReply::Handled();
                })
                [
                    SNew(SBox).Padding(FMargin(10, 6))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(NameLine))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
                            .ColorAndOpacity(FLinearColor(0.95f, 0.9f, 0.7f))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(DetailLine))
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
                            .ColorAndOpacity(ClassColor)
                        ]
                    ]
                ]
            ]
        ];
    }

    SetStatusText(FString::Printf(TEXT("%d character(s) — click to enter world"), Characters.Num()));
}

void SWowCharacterSelectWidget::SetPreviewRenderTarget(UTextureRenderTarget2D* InRenderTarget)
{
    if (!PreviewImage.IsValid()) return;

    if (InRenderTarget)
    {
        PreviewBrush = MakeShared<FSlateBrush>();
        PreviewBrush->SetResourceObject(InRenderTarget);
        PreviewBrush->ImageSize = FVector2D(512, 768);
        PreviewBrush->DrawAs = ESlateBrushDrawType::Image;
        PreviewBrush->Tiling = ESlateBrushTileType::NoTile;

        PreviewImage->SetImage(PreviewBrush.Get());
        PreviewImage->SetVisibility(EVisibility::SelfHitTestInvisible);

        if (PreviewPlaceholder.IsValid())
        {
            PreviewPlaceholder->SetVisibility(EVisibility::Collapsed);
        }
    }
}

void SWowCharacterSelectWidget::SetStatusText(const FString& Text)
{
    if (StatusLabel.IsValid()) StatusLabel->SetText(FText::FromString(Text));
}
