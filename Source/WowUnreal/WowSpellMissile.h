#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "WowSpellMissile.generated.h"

UCLASS()
class WOWUNREAL_API AWowSpellMissile : public AActor
{
    GENERATED_BODY()

public:
    AWowSpellMissile();

    /** Initialize the missile with start/end positions and spell school color */
    void Initialize(const FVector& StartLocation, const FVector& TargetLocation, const FLinearColor& SpellColor, float TravelTime = 1.0f);

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

protected:
    /** Sphere mesh component for the missile */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> SphereMesh;

    /** Dynamic material instance for changing color */
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

    /** Point light component for ambient glow */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UPointLightComponent> GlowLight;

private:
    FVector StartPos;
    FVector EndPos;
    float TotalTravelTime;
    float CurrentTime;
    bool bIsInitialized;

    void UpdatePosition();
    void OnReachedTarget();
};