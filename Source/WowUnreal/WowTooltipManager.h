#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "WowTooltipManager.generated.h"

class SWowTooltip;
class UWowConnectionManager;

// Forward declare from WowNetwork module
struct FWowEntity;

/**
 * Manages entity tooltips that appear on mouse hover
 */
UCLASS(BlueprintType)
class WOWUNREAL_API UWowTooltipManager : public UObject
{
    GENERATED_BODY()

public:
    UWowTooltipManager();

    /** Initialize with connection manager for entity data */
    void Initialize(UWowConnectionManager* ConnectionMgr, TSharedPtr<SWidget> RootWidget);

    /** Shutdown and cleanup */
    void Shutdown();

    /** Update tooltip position and content based on mouse position */
    void Update(const FVector2D& MousePosition);

    /** Show tooltip for specific entity */
    void ShowTooltip(uint64 EntityGuid, const FVector2D& Position);

    /** Show tooltip for specific entity (native interface) */
    void ShowTooltipForEntity(const FWowEntity* Entity, const FVector2D& Position);

    /** Hide tooltip */
    UFUNCTION(BlueprintCallable, Category = "Tooltip")
    void HideTooltip();

    /** Check if tooltip is currently visible */
    UFUNCTION(BlueprintCallable, Category = "Tooltip")
    bool IsTooltipVisible() const;

private:
    /** Find entity under cursor using line trace */
    const FWowEntity* FindEntityUnderCursor(const FVector2D& ScreenPosition) const;

    /** Get world from connection manager */
    UWorld* GetWorld() const;

    /** Connection manager for accessing entity data */
    UPROPERTY()
    TObjectPtr<UWowConnectionManager> ConnectionManager;

    /** Tooltip widget */
    TSharedPtr<SWowTooltip> TooltipWidget;

    /** Root widget for positioning */
    TWeakPtr<SWidget> RootWidgetPtr;

    /** Currently hovered entity */
    const FWowEntity* CurrentHoveredEntity = nullptr;

    /** Hover delay timer */
    float HoverTimer = 0.0f;

    /** Time required to hover before showing tooltip */
    float HoverDelay = 0.5f;

    /** Last mouse position */
    FVector2D LastMousePosition = FVector2D::ZeroVector;
};