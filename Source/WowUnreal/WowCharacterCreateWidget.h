#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_NineParams(FOnCharacterCreated,
    const FString& /*Name*/, uint8 /*Race*/, uint8 /*Class*/, uint8 /*Gender*/,
    uint8 /*Skin*/, uint8 /*Face*/, uint8 /*HairStyle*/, uint8 /*HairColor*/, uint8 /*FacialHair*/);
DECLARE_DELEGATE(FOnBackToCharSelect);

class SWowCharacterCreateWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowCharacterCreateWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void SetStatusText(const FString& Text);

    FOnCharacterCreated OnCharacterCreated;
    FOnBackToCharSelect OnBackToCharSelect;

private:
    TSharedPtr<SEditableTextBox> NameBox;
    TSharedPtr<STextBlock> StatusLabel;
    TSharedPtr<STextBlock> RaceText;
    TSharedPtr<STextBlock> ClassText;
    TSharedPtr<STextBlock> GenderText;
    int32 SelectedRaceIndex = 0;
    int32 SelectedClassIndex = 0;
    int32 SelectedGender = 0;
};
