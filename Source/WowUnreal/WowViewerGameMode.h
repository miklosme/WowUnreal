#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "WowViewerGameMode.generated.h"

class AWowWorldManager;
class AWowLoginController;

UCLASS()
class WOWUNREAL_API AWowViewerGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AWowViewerGameMode();
    virtual void BeginPlay() override;

private:
    void SetupPostProcess(UWorld* World);
    AWowWorldManager* SpawnWorldManager(UWorld* World);
    void SpawnGroundPlane(UWorld* World, const FVector& Center, float Size);
    void SpawnDirectionalLight(UWorld* World);

    void SetupDefaultScene(UWorld* World);
    void SetupCharacterTestScene(UWorld* World);
    void SetupTerrainTestScene(UWorld* World);
    void SetupWmoTestScene(UWorld* World);
    void SetupUITestScene(UWorld* World);
    void SetupLoginScene(UWorld* World);
};
