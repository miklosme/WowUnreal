#include "WowM2ParticleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Engine.h"
#include "UObject/ConstructorHelpers.h"
#include "ProceduralMeshComponent.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Formats/M2Types.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowParticles, Log, All);

UWowM2ParticleComponent::UWowM2ParticleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UWowM2ParticleComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UWowM2ParticleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearParticles();
    Super::EndPlay(EndPlayReason);
}

void UWowM2ParticleComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update fire effects
    for (FFireEffect& Effect : FireEffects)
    {
        UpdateFireEffect(Effect, DeltaTime);
    }

    // Update smoke effects
    for (FSmokeEffect& Effect : SmokeEffects)
    {
        UpdateSmokeEffect(Effect, DeltaTime);
    }

    // Update sparkle effects
    for (FSparkleEffect& Effect : SparkleEffects)
    {
        UpdateSparkleEffect(Effect, DeltaTime);
    }
}

void UWowM2ParticleComponent::InitializeFromM2Data(const TArray<uint8>& M2DataRaw)
{
    // TODO: Parse M2 data directly here if needed for blueprint support
    UE_LOG(LogWowParticles, Log, TEXT("Blueprint M2 particle initialization not implemented yet"));
}

void UWowM2ParticleComponent::InitializeFromParsedM2Data(const void* M2DataPtr)
{
    if (!M2DataPtr)
    {
        return;
    }

    const FM2Data* M2Data = static_cast<const FM2Data*>(M2DataPtr);
    if (M2Data->ParticleEmitters.Num() == 0)
    {
        return;
    }

    UE_LOG(LogWowParticles, Log, TEXT("Initializing %d particle emitters"), M2Data->ParticleEmitters.Num());

    // Clear existing particles
    ClearParticles();

    for (const FM2ParticleEmitter& Emitter : M2Data->ParticleEmitters)
    {
        // Calculate world position from bone attachment
        FVector WorldPosition = Emitter.Position;
        if (Emitter.Bone >= 0 && Emitter.Bone < M2Data->Bones.Num())
        {
            const FM2Bone& Bone = M2Data->Bones[Emitter.Bone];
            // For simplicity, just use bone pivot point + emitter offset
            // In a full implementation, you'd want to track bone transforms during animation
            WorldPosition = Bone.PivotPoint + Emitter.Position;
        }

        // Determine effect type and create appropriate components
        if (Emitter.IsFireEmitter())
        {
            CreateFireEffect(Emitter, WorldPosition);
            UE_LOG(LogWowParticles, Log, TEXT("Created fire effect at %s"), *WorldPosition.ToString());
        }
        else if (Emitter.IsSmokeEmitter())
        {
            CreateSmokeEffect(Emitter, WorldPosition);
            UE_LOG(LogWowParticles, Log, TEXT("Created smoke effect at %s"), *WorldPosition.ToString());
        }
        else if (Emitter.IsSparkleEmitter())
        {
            CreateSparkleEffect(Emitter, WorldPosition);
            UE_LOG(LogWowParticles, Log, TEXT("Created sparkle effect at %s"), *WorldPosition.ToString());
        }
        else
        {
            // Default to sparkle for unknown emitter types
            CreateSparkleEffect(Emitter, WorldPosition);
            UE_LOG(LogWowParticles, Log, TEXT("Created default sparkle effect at %s"), *WorldPosition.ToString());
        }
    }
}

void UWowM2ParticleComponent::ClearParticles()
{
    // Clean up fire effects
    for (FFireEffect& Effect : FireEffects)
    {
        if (Effect.Light)
        {
            Effect.Light->DestroyComponent();
        }
        if (Effect.Billboard)
        {
            Effect.Billboard->DestroyComponent();
        }
    }
    FireEffects.Empty();

    // Clean up smoke effects
    for (FSmokeEffect& Effect : SmokeEffects)
    {
        if (Effect.Billboard)
        {
            Effect.Billboard->DestroyComponent();
        }
    }
    SmokeEffects.Empty();

    // Clean up sparkle effects
    for (FSparkleEffect& Effect : SparkleEffects)
    {
        if (Effect.Billboard)
        {
            Effect.Billboard->DestroyComponent();
        }
    }
    SparkleEffects.Empty();
}

void UWowM2ParticleComponent::CreateFireEffect(const FM2ParticleEmitter& Emitter, const FVector& WorldPosition)
{
    FFireEffect Effect;
    Effect.BasePosition = WorldPosition;
    Effect.BaseIntensity = FMath::Clamp(Emitter.EmissionRate * 100.0f, 500.0f, 3000.0f);

    // Create point light for fire glow
    Effect.Light = NewObject<UPointLightComponent>(this);
    Effect.Light->SetupAttachment(this);
    Effect.Light->SetRelativeLocation(WorldPosition);
    Effect.Light->SetLightColor(FLinearColor(1.0f, 0.6f, 0.2f)); // Orange fire color
    Effect.Light->SetIntensity(Effect.BaseIntensity);
    Effect.Light->SetAttenuationRadius(200.0f);
    Effect.Light->SetCastShadows(false); // Performance optimization
    Effect.Light->RegisterComponent();

    // Create billboard for fire sprite (if we have a billboard mesh)
    UStaticMesh* BillboardQuad = GetBillboardMesh();
    if (BillboardQuad)
    {
        Effect.Billboard = NewObject<UStaticMeshComponent>(this);
        Effect.Billboard->SetupAttachment(this);
        Effect.Billboard->SetRelativeLocation(WorldPosition);
        Effect.Billboard->SetStaticMesh(BillboardQuad);
        Effect.Billboard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Effect.Billboard->SetCastShadow(false);

        // Create material for fire sprite
        UMaterial* BaseMat = GetBaseBillboardMaterial();
        if (BaseMat)
        {
            Effect.Material = UMaterialInstanceDynamic::Create(BaseMat, this);
            Effect.Material->SetVectorParameterValue(FName("Color"), FLinearColor(1.0f, 0.4f, 0.1f, 0.8f));
            Effect.Material->SetScalarParameterValue(FName("Emissive"), 2.0f);
            Effect.Billboard->SetMaterial(0, Effect.Material);
        }

        Effect.Billboard->RegisterComponent();
    }

    FireEffects.Add(Effect);
}

void UWowM2ParticleComponent::CreateSmokeEffect(const FM2ParticleEmitter& Emitter, const FVector& WorldPosition)
{
    FSmokeEffect Effect;
    Effect.BasePosition = WorldPosition;
    Effect.LifeSpan = FMath::Clamp(Emitter.HeadLifeStart / 1000.0f, 2.0f, 10.0f); // Convert ms to seconds
    Effect.RiseVelocity = FVector(0, 0, FMath::Abs(Emitter.Gravity) * 5.0f); // Rise opposite to gravity

    // Create billboard for smoke sprite
    UStaticMesh* BillboardQuad = GetBillboardMesh();
    if (BillboardQuad)
    {
        Effect.Billboard = NewObject<UStaticMeshComponent>(this);
        Effect.Billboard->SetupAttachment(this);
        Effect.Billboard->SetRelativeLocation(WorldPosition);
        Effect.Billboard->SetStaticMesh(BillboardQuad);
        Effect.Billboard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Effect.Billboard->SetCastShadow(false);

        // Create material for smoke
        UMaterial* BaseMat = GetBaseBillboardMaterial();
        if (BaseMat)
        {
            Effect.Material = UMaterialInstanceDynamic::Create(BaseMat, this);
            Effect.Material->SetVectorParameterValue(FName("Color"), FLinearColor(0.3f, 0.3f, 0.3f, 0.6f));
            Effect.Material->SetScalarParameterValue(FName("Emissive"), 0.1f);
            Effect.Billboard->SetMaterial(0, Effect.Material);
        }

        Effect.Billboard->RegisterComponent();
    }

    SmokeEffects.Add(Effect);
}

void UWowM2ParticleComponent::CreateSparkleEffect(const FM2ParticleEmitter& Emitter, const FVector& WorldPosition)
{
    FSparkleEffect Effect;
    Effect.BasePosition = WorldPosition;
    Effect.PulseRate = FMath::Clamp(Emitter.EmissionRate / 5.0f, 1.0f, 4.0f);

    // Create billboard for sparkle
    UStaticMesh* BillboardQuad = GetBillboardMesh();
    if (BillboardQuad)
    {
        Effect.Billboard = NewObject<UStaticMeshComponent>(this);
        Effect.Billboard->SetupAttachment(this);
        Effect.Billboard->SetRelativeLocation(WorldPosition);
        Effect.Billboard->SetStaticMesh(BillboardQuad);
        Effect.Billboard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Effect.Billboard->SetCastShadow(false);

        // Set smaller scale for sparkles
        Effect.Billboard->SetRelativeScale3D(FVector(0.3f, 0.3f, 0.3f));

        // Create material for sparkle
        UMaterial* BaseMat = GetBaseBillboardMaterial();
        if (BaseMat)
        {
            Effect.Material = UMaterialInstanceDynamic::Create(BaseMat, this);
            // Use bright colors based on emitter data
            float R = Emitter.ColorStart[0] / 255.0f;
            float G = Emitter.ColorStart[1] / 255.0f;
            float B = Emitter.ColorStart[2] / 255.0f;
            Effect.Material->SetVectorParameterValue(FName("Color"), FLinearColor(R, G, B, 0.9f));
            Effect.Material->SetScalarParameterValue(FName("Emissive"), 3.0f);
            Effect.Billboard->SetMaterial(0, Effect.Material);
        }

        Effect.Billboard->RegisterComponent();
    }

    SparkleEffects.Add(Effect);
}

void UWowM2ParticleComponent::UpdateFireEffect(FFireEffect& Effect, float DeltaTime)
{
    if (!Effect.Light && !Effect.Billboard)
    {
        return;
    }

    Effect.FlickerTime += DeltaTime;

    // Flickering light intensity
    if (Effect.Light)
    {
        float FlickerScale = 1.0f + 0.3f * FMath::Sin(Effect.FlickerTime * 8.0f) +
                           0.1f * FMath::Sin(Effect.FlickerTime * 15.0f);
        Effect.Light->SetIntensity(Effect.BaseIntensity * FlickerScale);
    }

    // Animate fire sprite opacity
    if (Effect.Billboard && Effect.Material)
    {
        float OpacityScale = 1.0f + 0.2f * FMath::Sin(Effect.FlickerTime * 6.0f);
        FLinearColor CurrentColor;
        Effect.Material->GetVectorParameterValue(FName("Color"), CurrentColor);
        CurrentColor.A = 0.8f * OpacityScale;
        Effect.Material->SetVectorParameterValue(FName("Color"), CurrentColor);
    }
}

void UWowM2ParticleComponent::UpdateSmokeEffect(FSmokeEffect& Effect, float DeltaTime)
{
    if (!Effect.Billboard)
    {
        return;
    }

    Effect.FadeTime += DeltaTime;

    // Move smoke upward
    FVector CurrentPos = Effect.Billboard->GetRelativeLocation();
    CurrentPos += Effect.RiseVelocity * DeltaTime;
    Effect.Billboard->SetRelativeLocation(CurrentPos);

    // Fade out over time
    if (Effect.Material)
    {
        float Alpha = 1.0f - (Effect.FadeTime / Effect.LifeSpan);
        Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

        FLinearColor CurrentColor;
        Effect.Material->GetVectorParameterValue(FName("Color"), CurrentColor);
        CurrentColor.A = 0.6f * Alpha;
        Effect.Material->SetVectorParameterValue(FName("Color"), CurrentColor);
    }

    // Reset when fully faded
    if (Effect.FadeTime >= Effect.LifeSpan)
    {
        Effect.FadeTime = 0.0f;
        Effect.Billboard->SetRelativeLocation(Effect.BasePosition);
    }
}

void UWowM2ParticleComponent::UpdateSparkleEffect(FSparkleEffect& Effect, float DeltaTime)
{
    if (!Effect.Billboard || !Effect.Material)
    {
        return;
    }

    Effect.PulseTime += DeltaTime;

    // Pulsing emissive intensity
    float PulseScale = 0.5f + 0.5f * FMath::Sin(Effect.PulseTime * Effect.PulseRate * PI * 2.0f);
    Effect.Material->SetScalarParameterValue(FName("Emissive"), 3.0f * PulseScale);
}

UStaticMesh* UWowM2ParticleComponent::GetBillboardMesh()
{
    if (BillboardMesh)
    {
        return BillboardMesh;
    }

    // For now, return nullptr and log that we need a billboard mesh
    // In a full implementation, you'd create a simple quad mesh procedurally
    // or load one from the engine's built-in assets
    UE_LOG(LogWowParticles, Warning, TEXT("Billboard mesh not available - particle sprites will be disabled"));
    return nullptr;
}

UMaterial* UWowM2ParticleComponent::GetBaseBillboardMaterial()
{
    if (BaseBillboardMaterial)
    {
        return BaseBillboardMaterial;
    }

    // Try to load a basic unlit material from the engine
    BaseBillboardMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    if (!BaseBillboardMaterial)
    {
        UE_LOG(LogWowParticles, Warning, TEXT("Could not load base billboard material"));
    }

    return BaseBillboardMaterial;
}