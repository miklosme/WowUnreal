#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "WowSessionState.h"

DECLARE_DELEGATE_OneParam(FOnRealmSelected, int32 /*Index*/);

class SWowRealmSelectWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowRealmSelectWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void PopulateRealms(const TArray<FWowRealmInfo>& Realms);
    void SetStatusText(const FString& Text);

    FOnRealmSelected OnRealmSelected;

private:
    TSharedPtr<SVerticalBox> RealmListBox;
    TSharedPtr<STextBlock> StatusLabel;
};
