#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Brushes/SlateImageBrush.h"

class FWowEntityManager;
class AWowWorldManager;
struct FWowEntity;

/**
 * WoW-style minimap widget showing player position, entity dots, and zone name.
 * Positioned in the top-right corner with a circular gold border.
 */
class WOWUNREAL_API SWowMinimap : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowMinimap)
    {
        _Visibility = EVisibility::Visible;
    }
        SLATE_ARGUMENT(FWowEntityManager*, EntityManager)
        SLATE_ARGUMENT(AWowWorldManager*, WorldManager)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Update minimap data (called each frame or every 0.5s) */
    void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

    /** Toggle visibility */
    void SetMinimapVisibility(EVisibility InVisibility);

    /** Handle mouse wheel for zoom */
    virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

    /** Handle 'M' key for full-screen map */
    void ToggleFullScreenMap();

private:
    /** Entity manager for reading entity positions */
    FWowEntityManager* EntityManager;

    /** World manager for current area/zone */
    TWeakObjectPtr<AWowWorldManager> WorldManager;

    /** Minimap parameters */
    static constexpr float MinimapSize = 200.0f;
    static constexpr float DefaultZoomYards = 100.0f;
    static constexpr float MinZoomYards = 50.0f;
    static constexpr float MaxZoomYards = 500.0f;

    /** Current zoom level in WoW yards */
    float ZoomYards = DefaultZoomYards;

    /** Whether full-screen map is shown */
    bool bIsFullScreen = false;

    /** Zone name text widget */
    TSharedPtr<class STextBlock> ZoneNameText;

    /** Custom paint for the minimap circle and entities */
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                         const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                         int32 LayerId, const FWidgetStyle& InWidgetStyle,
                         bool bParentEnabled) const override;

    /** Paint the circular minimap background */
    void PaintMinimapBackground(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
                               int32 LayerId, const FVector2D& Center, float Radius) const;

    /** Paint entity dots */
    void PaintEntityDots(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
                        int32 LayerId, const FVector2D& Center, float Radius) const;

    /** Paint the player arrow */
    void PaintPlayerArrow(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
                         int32 LayerId, const FVector2D& Center) const;

    /** Paint the circular gold border */
    void PaintCircularBorder(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements,
                            int32 LayerId, const FVector2D& Center, float Radius) const;

    /** Convert world position to minimap coordinates */
    FVector2D WorldToMinimapCoords(const FVector& WorldPos, const FVector& PlayerPos,
                                  const FVector2D& Center, float Radius, float PlayerYaw) const;

    /** Get entity color based on type/faction */
    FLinearColor GetEntityColor(const FWowEntity& Entity) const;

    /** Get current zone name */
    FString GetCurrentZoneName() const;

    /** Get current player position and rotation */
    bool GetPlayerPositionAndRotation(FVector& OutPosition, float& OutYaw) const;

    /** Check if position is within minimap circle */
    bool IsWithinMinimapCircle(const FVector2D& Center, float Radius, const FVector2D& Point) const;

    /** Last update time for performance throttling */
    mutable double LastUpdateTime = 0.0;
    mutable float UpdateInterval = 0.5f; // Update every 0.5 seconds
};