#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "WowSessionState.h"

DECLARE_DELEGATE_OneParam(FOnCharacterSelected, int64 /*Guid*/);
DECLARE_DELEGATE(FOnCreateCharacterRequest);
DECLARE_DELEGATE_OneParam(FOnDeleteCharacterRequest, int64 /*Guid*/);

class SWowCharacterSelectWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowCharacterSelectWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void PopulateCharacters(const TArray<FWowCharacterInfo>& Characters);
    void SetStatusText(const FString& Text);

    FOnCharacterSelected OnCharacterSelected;
    FOnCreateCharacterRequest OnCreateCharacterRequest;
    FOnDeleteCharacterRequest OnDeleteCharacterRequest;

private:
    TSharedPtr<SVerticalBox> CharListBox;
    TSharedPtr<STextBlock> StatusLabel;
    TArray<FWowCharacterInfo> CachedCharacters;
};
