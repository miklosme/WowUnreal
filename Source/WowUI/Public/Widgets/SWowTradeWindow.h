#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "WowEntity.h"

class STextBlock;
class SVerticalBox;

DECLARE_DELEGATE_OneParam(FOnTradeAccept, bool /*bBeginTrade*/);
DECLARE_DELEGATE(FOnTradeCancel);

class WOWUI_API SWowTradeWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowTradeWindow) {}
        SLATE_EVENT(FOnTradeAccept, OnTradeAccept)
        SLATE_EVENT(FOnTradeCancel, OnTradeCancel)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    void UpdateTrade(const FWowTradeState& TradeState);
    void CloseTrade();

private:
    FOnTradeAccept OnTradeAccept;
    FOnTradeCancel OnTradeCancel;

    FWowTradeState CurrentTrade;
    TSharedPtr<STextBlock> StatusText;
    TSharedPtr<STextBlock> PlayerMoneyText;
    TSharedPtr<STextBlock> TargetMoneyText;
    TSharedPtr<SVerticalBox> PlayerItemsBox;
    TSharedPtr<SVerticalBox> TargetItemsBox;

    FReply OnAcceptClicked();
    FReply OnCancelClicked();
    void RefreshTradeLists();
    FText GetStatusText() const;
    FText GetAcceptButtonText() const;
    FText GetMoneyText(uint32 Copper) const;
    FString GetTraderLabel() const;
};
