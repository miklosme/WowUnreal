#include "WowSpellMissile.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Components/PointLightComponent.h"

AWowSpellMissile::AWowSpellMissile()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create sphere mesh component
    SphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SphereMesh"));
    RootComponent = SphereMesh;

    // Load sphere mesh
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshAsset(TEXT("/Engine/BasicShapes/Sphere"));
    if (SphereMeshAsset.Succeeded())
    {
        SphereMesh->SetStaticMesh(SphereMeshAsset.Object.Get());
    }

    // Scale down the sphere for better visibility
    SphereMesh->SetWorldScale3D(FVector(0.5f, 0.5f, 0.5f));

    // Create point light component for ambient glow
    GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
    GlowLight->SetupAttachment(SphereMesh);
    GlowLight->SetIntensity(3000.0f);
    GlowLight->SetAttenuationRadius(200.0f);
    GlowLight->SetCastShadows(false); // Disable shadows for performance

    // Initialize variables
    StartPos = FVector::ZeroVector;
    EndPos = FVector::ZeroVector;
    TotalTravelTime = 1.0f;
    CurrentTime = 0.0f;
    bIsInitialized = false;

    // Disable collision
    SphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AWowSpellMissile::Initialize(const FVector& StartLocation, const FVector& TargetLocation, const FLinearColor& SpellColor, float TravelTime)
{
    StartPos = StartLocation;
    EndPos = TargetLocation;
    TotalTravelTime = FMath::Max(0.1f, TravelTime); // Minimum 0.1 second travel time
    CurrentTime = 0.0f;
    bIsInitialized = true;

    // Set initial position
    SetActorLocation(StartPos);

    // Set glow light color to match spell school
    if (GlowLight)
    {
        GlowLight->SetLightColor(SpellColor);
    }

#if WITH_EDITOR
    // Create proper emissive material at runtime for glow effect
    UMaterial* MissileMat = NewObject<UMaterial>(GetTransientPackage());
    MissileMat->SetShadingModel(MSM_Unlit);
    MissileMat->BlendMode = BLEND_Additive; // Additive for glow effect

    auto* ColorParam = NewObject<UMaterialExpressionConstant3Vector>(MissileMat);
    ColorParam->Constant = FLinearColor(SpellColor.R, SpellColor.G, SpellColor.B); // The spell school color
    MissileMat->GetExpressionCollection().AddExpression(ColorParam);

    auto* Multiply = NewObject<UMaterialExpressionMultiply>(MissileMat);
    Multiply->ConstB = 5.0f; // Intensity multiplier for glow
    MissileMat->GetExpressionCollection().AddExpression(Multiply);
    Multiply->A.Connect(0, ColorParam);

    MissileMat->GetEditorOnlyData()->EmissiveColor.Connect(0, Multiply);
    MissileMat->GetEditorOnlyData()->Opacity.Connect(0, ColorParam); // Use color brightness as opacity

    MissileMat->PreEditChange(nullptr);
    MissileMat->PostEditChange();

    SphereMesh->SetMaterial(0, MissileMat);
#else
    // Fallback for runtime without editor - use basic material with dynamic instance
    static ConstructorHelpers::FObjectFinder<UMaterial> UnlitMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    if (UnlitMaterial.Succeeded())
    {
        DynamicMaterial = UMaterialInstanceDynamic::Create(UnlitMaterial.Object.Get(), this);
        if (DynamicMaterial)
        {
            DynamicMaterial->SetVectorParameterValue(FName("Color"), SpellColor);
            SphereMesh->SetMaterial(0, DynamicMaterial);
        }
    }
#endif
}

void AWowSpellMissile::BeginPlay()
{
    Super::BeginPlay();
}

void AWowSpellMissile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsInitialized)
    {
        return;
    }

    CurrentTime += DeltaTime;

    if (CurrentTime >= TotalTravelTime)
    {
        // Reached target
        OnReachedTarget();
    }
    else
    {
        // Update position
        UpdatePosition();
    }
}

void AWowSpellMissile::UpdatePosition()
{
    if (!bIsInitialized)
    {
        return;
    }

    // Simple linear interpolation
    float Alpha = CurrentTime / TotalTravelTime;
    Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

    FVector NewPosition = FMath::Lerp(StartPos, EndPos, Alpha);
    SetActorLocation(NewPosition);
}

void AWowSpellMissile::OnReachedTarget()
{
    // Could add impact effect here in the future
    UE_LOG(LogTemp, Log, TEXT("Spell missile reached target"));

    // Destroy the missile
    Destroy();
}