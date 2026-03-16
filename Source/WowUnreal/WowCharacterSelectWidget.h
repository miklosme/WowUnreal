#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "WowSessionState.h"

DECLARE_DELEGATE_OneParam(FOnCharacterSelected, int64 /*Guid*/);
DECLARE_DELEGATE(FOnCreateCharacterRequest);
DECLARE_DELEGATE_OneParam(FOnDeleteCharacterRequest, int64 /*Guid*/);
DECLARE_DELEGATE_TwoParams(FOnCharacterHighlighted, uint8 /*Race*/, uint8 /*Gender*/);

class UTextureRenderTarget2D;
struct FSlateBrush;

class SWowCharacterSelectWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowCharacterSelectWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void PopulateCharacters(const TArray<FWowCharacterInfo>& Characters);
    void SetStatusText(const FString& Text);

    /** Set the render target for 3D character preview */
    void SetPreviewRenderTarget(UTextureRenderTarget2D* InRenderTarget);

    FOnCharacterSelected OnCharacterSelected;
    FOnCreateCharacterRequest OnCreateCharacterRequest;
    FOnDeleteCharacterRequest OnDeleteCharacterRequest;
    FOnCharacterHighlighted OnCharacterHighlighted;

private:
    TSharedPtr<SVerticalBox> CharListBox;
    TSharedPtr<STextBlock> StatusLabel;
    TSharedPtr<SImage> PreviewImage;
    TSharedPtr<FSlateBrush> PreviewBrush;
    TSharedPtr<STextBlock> PreviewPlaceholder;
    TArray<FWowCharacterInfo> CachedCharacters;
};
