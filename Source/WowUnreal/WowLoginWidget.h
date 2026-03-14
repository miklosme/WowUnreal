#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

DECLARE_DELEGATE_FourParams(FOnLoginSubmit, const FString& /*Server*/, int32 /*Port*/, const FString& /*User*/, const FString& /*Pass*/);

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

    TSharedPtr<SEditableTextBox> ServerBox;
    TSharedPtr<SEditableTextBox> UsernameBox;
    TSharedPtr<SEditableTextBox> PasswordBox;
    TSharedPtr<STextBlock> StatusLabel;
};
