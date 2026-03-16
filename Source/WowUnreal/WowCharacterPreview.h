#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WowSessionState.h"
#include "WowCharacterPreview.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UDirectionalLightComponent;
class UPointLightComponent;
class AWowWorldManager;

/**
 * Renders a character model to a render target for UI preview.
 * Placed at a distant location to isolate from the main world.
 */
UCLASS()
class WOWUNREAL_API AWowCharacterPreview : public AActor
{
    GENERATED_BODY()
public:
    AWowCharacterPreview();
    virtual void BeginPlay() override;

    /** Initialize the capture system */
    void Setup(AWowWorldManager* InWorldManager, int32 Width = 512, int32 Height = 768);

    /** Show a character by race/gender (from character info) */
    void ShowCharacter(uint8 RaceId, uint8 GenderId);

    /** Clear the current preview */
    void ClearCharacter();

    /** Manually capture the scene */
    void CaptureNow();

    /** Get the render target texture for Slate display */
    UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

private:
    void PositionCamera(uint8 RaceId);

    UPROPERTY()
    TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

    UPROPERTY()
    TObjectPtr<UTextureRenderTarget2D> RenderTarget;

    UPROPERTY()
    TObjectPtr<UDirectionalLightComponent> KeyLight;

    UPROPERTY()
    TObjectPtr<UPointLightComponent> FillLight;

    UPROPERTY()
    TObjectPtr<UPointLightComponent> RimLight;

    UPROPERTY()
    TObjectPtr<AActor> CurrentCharacterActor;

    UPROPERTY()
    TObjectPtr<AWowWorldManager> WorldManager;

    /** Offset from world origin for isolation */
    static const FVector PREVIEW_OFFSET;
};
