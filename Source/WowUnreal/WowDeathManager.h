#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "WowDeathManager.generated.h"

class SWowDeathScreen;
class UWowConnectionManager;

/**
 * Manages player death state and UI
 */
UCLASS(BlueprintType)
class WOWUNREAL_API UWowDeathManager : public UObject
{
    GENERATED_BODY()

public:
    UWowDeathManager();

    /** Initialize with connection manager and root widget */
    void Initialize(UWowConnectionManager* ConnectionMgr, TSharedPtr<SWidget> RootWidget);

    /** Shutdown and cleanup */
    void Shutdown();

    /** Check if player is currently dead */
    UPROPERTY(BlueprintReadOnly, Category = "Death")
    bool bIsPlayerDead = false;

    /** Show the death screen */
    UFUNCTION(BlueprintCallable, Category = "Death")
    void ShowDeathScreen();

    /** Hide the death screen */
    UFUNCTION(BlueprintCallable, Category = "Death")
    void HideDeathScreen();

    /** Handle release spirit button click */
    void OnReleaseSpirit();

private:
    /** Event handlers */
    void OnPlayerDeath();
    void OnCorpseReclaimDelay(float DelayInSeconds);
    void OnResurrectRequest(const FString& RequesterName);
    void OnPlayerTeleport(uint32 MapId, float X, float Y, float Z);

    /** Connection manager for sending packets */
    UPROPERTY()
    TObjectPtr<UWowConnectionManager> ConnectionManager;

    /** Death screen widget */
    TSharedPtr<SWowDeathScreen> DeathScreenWidget;

    /** Root widget for overlay */
    TWeakPtr<SWidget> RootWidgetPtr;
};