#pragma once
#include "CoreMinimal.h"
#include "WowTestGameMode.h"
#include "WowModelViewerGameMode.generated.h"

class SWowModelViewerWidget;
class AWowOrbitCamera;

/**
 * Game mode for the WoW Model Viewer.
 * Spawns a single character/creature, orbit camera, and Slate UI panel.
 */
UCLASS()
class WOWUNREAL_API AWowModelViewerGameMode : public AWowTestGameMode
{
    GENERATED_BODY()
public:
    AWowModelViewerGameMode();

protected:
    virtual void SetupTestScene(UWorld* World, AWowWorldManager* WorldManager) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void CreateUI();
    void RespawnCharacter();
    void SpawnCreature(uint32 DisplayId);
    void DestroyCurrentModel();

    UPROPERTY() TObjectPtr<AActor> CurrentModelActor;
    UPROPERTY() TObjectPtr<AWowWorldManager> WorldMgr;

    TSharedPtr<SWowModelViewerWidget> ViewerWidget;
    TSharedPtr<SWidget> ViewerWidgetContainer;
};
