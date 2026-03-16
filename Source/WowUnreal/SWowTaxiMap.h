#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SCanvas.h"
#include "WowEntity.h"

class UWowConnectionManager;

/**
 * Taxi map widget that shows available flight paths
 * Full-screen overlay with clickable taxi nodes
 */
class WOWUNREAL_API SWowTaxiMap : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowTaxiMap)
        {}
        SLATE_ARGUMENT(UWowConnectionManager*, ConnectionManager)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Show the taxi map with available nodes */
    void ShowTaxiMap(const FWowTaxiData& TaxiData);

    /** Close the taxi map */
    void CloseTaxiMap();

private:
    struct FTaxiNodeWidget
    {
        uint32 NodeId = 0;
        FWowTaxiNode NodeData;
        bool bKnown = false;
        TSharedPtr<SButton> NodeButton;
        TSharedPtr<STextBlock> NameLabel;
    };

    /** Connection manager for sending taxi commands */
    TWeakObjectPtr<UWowConnectionManager> ConnectionManager;

    /** Current taxi data */
    FWowTaxiData CurrentTaxiData;

    /** Taxi node widgets */
    TArray<FTaxiNodeWidget> NodeWidgets;

    /** Canvas for positioning taxi nodes */
    TSharedPtr<SCanvas> NodesCanvas;

    /** Background border */
    TSharedPtr<SBorder> Background;

    /** Close button */
    TSharedPtr<SButton> CloseButton;

    /** Create a taxi node widget */
    TSharedRef<SWidget> CreateNodeWidget(const FWowTaxiNode& Node, bool bKnown);

    /** Handle taxi node clicked */
    FReply OnNodeClicked(uint32 NodeId);

    /** Handle close button clicked */
    FReply OnCloseClicked();

    /** Handle escape key pressed */
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    /** Convert world coordinates to canvas position */
    FVector2D WorldToCanvas(float X, float Y) const;

    /** Get node button color */
    FSlateColor GetNodeButtonColor(uint32 NodeId) const;

    /** Get node button tooltip */
    FText GetNodeTooltipText(uint32 NodeId) const;

    /** Check if widget has keyboard focus */
    virtual bool SupportsKeyboardFocus() const override { return true; }
};