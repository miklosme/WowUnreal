#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WowM2ParticleComponent.generated.h"

/** Simple fire effect: flickering point light + emissive billboard */
USTRUCT()
struct FFireEffect
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UPointLightComponent> Light;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Billboard;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> Material;

    float FlickerTime = 0.0f;
    float BaseIntensity = 1000.0f;
    FVector BasePosition = FVector::ZeroVector;
};

/** Simple smoke effect: fading billboard quad */
USTRUCT()
struct FSmokeEffect
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Billboard;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> Material;

    float FadeTime = 0.0f;
    float LifeSpan = 3.0f;
    FVector RiseVelocity = FVector(0, 0, 50); // Rise upward
    FVector BasePosition = FVector::ZeroVector;
};

/** Simple sparkle effect: small emissive billboard */
USTRUCT()
struct FSparkleEffect
{
    GENERATED_BODY()

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> Billboard;

    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> Material;

    float PulseTime = 0.0f;
    float PulseRate = 2.0f; // Pulses per second
    FVector BasePosition = FVector::ZeroVector;
};

/**
 * Simple particle system component for WoW M2 particle emitters.
 * Instead of complex Niagara systems, this uses basic UE components:
 * - Point lights for fire effects (flickering orange light)
 * - Billboard quads for smoke (fading grey sprites)
 * - Small emissive quads for sparkles/ambient effects
 */
UCLASS(ClassGroup=(WowAssets), meta=(BlueprintSpawnableComponent))
class WOWASSETS_API UWowM2ParticleComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UWowM2ParticleComponent();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                             FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * Initialize particle effects from M2 particle emitter data.
     * This will create appropriate UE components based on emitter type.
     */
    UFUNCTION(BlueprintCallable, Category = "WoW Particles")
    void InitializeFromM2Data(const TArray<uint8>& M2DataRaw);

    /** Native initialization function using parsed M2 data (not blueprintable) */
    void InitializeFromParsedM2Data(const void* M2Data);

    /**
     * Clear all particle effects
     */
    UFUNCTION(BlueprintCallable, Category = "WoW Particles")
    void ClearParticles();

protected:
    /** Active particle effects */
    UPROPERTY()
    TArray<FFireEffect> FireEffects;

    UPROPERTY()
    TArray<FSmokeEffect> SmokeEffects;

    UPROPERTY()
    TArray<FSparkleEffect> SparkleEffects;

    /** Materials used for particle effects */
    UPROPERTY()
    TObjectPtr<UMaterial> BaseBillboardMaterial;

    /** Cached billboard mesh (quad) */
    UPROPERTY()
    TObjectPtr<UStaticMesh> BillboardMesh;

private:
    /** Create a fire effect from M2 emitter data */
    void CreateFireEffect(const struct FM2ParticleEmitter& Emitter, const FVector& WorldPosition);

    /** Create a smoke effect from M2 emitter data */
    void CreateSmokeEffect(const struct FM2ParticleEmitter& Emitter, const FVector& WorldPosition);

    /** Create a sparkle effect from M2 emitter data */
    void CreateSparkleEffect(const struct FM2ParticleEmitter& Emitter, const FVector& WorldPosition);

    /** Update fire effect animation */
    void UpdateFireEffect(FFireEffect& Effect, float DeltaTime);

    /** Update smoke effect animation */
    void UpdateSmokeEffect(FSmokeEffect& Effect, float DeltaTime);

    /** Update sparkle effect animation */
    void UpdateSparkleEffect(FSparkleEffect& Effect, float DeltaTime);

    /** Get or create the billboard mesh (simple quad) */
    UStaticMesh* GetBillboardMesh();

    /** Get or create base billboard material */
    UMaterial* GetBaseBillboardMaterial();
};