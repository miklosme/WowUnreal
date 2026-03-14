#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WowTestGameMode.generated.h"

class AWowWorldManager;

/**
 * Base game mode for test maps.
 * Sets up MpqManager + fly camera + PostProcess. No UI, no networking.
 * Test-specific GameModes inherit from this and override SetupTestScene().
 */
UCLASS()
class WOWUNREAL_API AWowTestGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AWowTestGameMode();
    virtual void BeginPlay() override;

protected:
    /** Override in subclasses to set up the specific test scene. Called after MPQ + PostProcess init. */
    virtual void SetupTestScene(UWorld* World, AWowWorldManager* WorldManager);

    /** Shared helpers for test scenes */
    void SetupPostProcess(UWorld* World);
    AWowWorldManager* SpawnWorldManager(UWorld* World);
    void SpawnGroundPlane(UWorld* World, const FVector& Center, float Size);
    void SpawnDirectionalLight(UWorld* World);
    void SpawnSkyManager(UWorld* World);

    UPROPERTY() TObjectPtr<AWowWorldManager> CachedWorldManager;
};
