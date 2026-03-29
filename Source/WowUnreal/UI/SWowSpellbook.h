#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

class UWowConnectionManager;
struct FSpellDbcEntry;
class AWowGameplayController;

class WOWUNREAL_API SWowSpellbook : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowSpellbook) {}
        SLATE_ARGUMENT(UWowConnectionManager*, ConnectionManager)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Set visibility of the spellbook window */
    void SetSpellbookVisible(bool bVisible);

    /** Check if spellbook is currently visible */
    bool IsSpellbookVisible() const { return bIsVisible; }

private:
    /** Connection manager for spell data and casting */
    TWeakObjectPtr<UWowConnectionManager> ConnectionManager;

    /** Main content widget */
    TSharedPtr<SWidget> SpellbookContent;

    /** Scroll box containing spell list */
    TSharedPtr<SScrollBox> SpellListScrollBox;

    /** Current visibility state */
    bool bIsVisible = false;

    /** Rebuild the spell list from known spells */
    void RefreshSpellList();

    /** Create a single spell row widget */
    TSharedRef<SWidget> CreateSpellRowWidget(uint32 SpellId);

    /** Handle spell row clicked */
    FReply OnSpellRowClicked(uint32 SpellId);

    /** Handle close button clicked */
    FReply OnCloseButtonClicked();

    /** Get spell data from DBC */
    const FSpellDbcEntry* GetSpellData(uint32 SpellId) const;

    /** Get spell school color based on school mask */
    FLinearColor GetSpellSchoolColor(uint32 SchoolMask) const;

    /** Format spell rank text */
    FString FormatSpellRank(const FSpellDbcEntry* SpellEntry) const;
};