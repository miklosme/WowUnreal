#include "WowTestGameMode.h"
#include "WowFlyCamera.h"
#include "WowDebugHUD.h"
#include "WowWorldManager.h"
#include "WowSkyManager.h"
#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowTestMode, Log, All);

AWowTestGameMode::AWowTestGameMode()
{
    DefaultPawnClass = AWowFlyCamera::StaticClass();
    HUDClass = AWowDebugHUD::StaticClass();
}

void AWowTestGameMode::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (!World) return;

    SetupPostProcess(World);

    CachedWorldManager = SpawnWorldManager(World);

    SetupTestScene(World, CachedWorldManager);
}

void AWowTestGameMode::SetupTestScene(UWorld* World, AWowWorldManager* WorldManager)
{
    // Base implementation does nothing — subclasses override this
    UE_LOG(LogWowTestMode, Log, TEXT("Base test scene — no additional setup"));
}

void AWowTestGameMode::SetupPostProcess(UWorld* World)
{
    if (IConsoleVariable* PreExpCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride")))
    {
        PreExpCVar->Set(1.0f);
    }

    APostProcessVolume* PPV = World->SpawnActor<APostProcessVolume>();
    if (PPV)
    {
        PPV->bUnbound = true;
        PPV->Settings.bOverride_AutoExposureMethod = true;
        PPV->Settings.AutoExposureMethod = AEM_Manual;
        PPV->Settings.bOverride_AutoExposureBias = true;
        PPV->Settings.AutoExposureBias = 0.0f;
        PPV->Settings.bOverride_AutoExposureMinBrightness = true;
        PPV->Settings.AutoExposureMinBrightness = 1.0f;
        PPV->Settings.bOverride_AutoExposureMaxBrightness = true;
        PPV->Settings.AutoExposureMaxBrightness = 1.0f;
    }
}

AWowWorldManager* AWowTestGameMode::SpawnWorldManager(UWorld* World)
{
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(World, AWowWorldManager::StaticClass(), Found);

    if (Found.Num() > 0)
    {
        return Cast<AWowWorldManager>(Found[0]);
    }

    FActorSpawnParameters Params;
    Params.Name = FName(TEXT("WowWorldManager"));
    AWowWorldManager* WM = World->SpawnActor<AWowWorldManager>(
        AWowWorldManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (WM)
    {
        UE_LOG(LogWowTestMode, Log, TEXT("Spawned WowWorldManager for MPQ access"));
    }
    return WM;
}

void AWowTestGameMode::SpawnGroundPlane(UWorld* World, const FVector& Center, float Size)
{
    UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (!PlaneMesh) return;

    AStaticMeshActor* PlaneActor = World->SpawnActor<AStaticMeshActor>(
        AStaticMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, Center));
    if (!PlaneActor) return;

    PlaneActor->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
    PlaneActor->SetActorScale3D(FVector(Size, Size, 1.0f));
    PlaneActor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PlaneActor->GetStaticMeshComponent()->SetCollisionObjectType(ECC_WorldStatic);
    PlaneActor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
}

void AWowTestGameMode::SpawnDirectionalLight(UWorld* World)
{
    ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(
        ADirectionalLight::StaticClass(), FTransform(FRotator(-45.0f, 30.0f, 0.0f), FVector::ZeroVector));
    if (Sun)
    {
        Sun->GetLightComponent()->SetIntensity(3.14159f);
        Sun->GetLightComponent()->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f));
    }
}

void AWowTestGameMode::SpawnSkyManager(UWorld* World)
{
    FActorSpawnParameters SkyParams;
    SkyParams.Name = FName(TEXT("WowSkyManager"));
    World->SpawnActor<AWowSkyManager>(AWowSkyManager::StaticClass(),
        FVector::ZeroVector, FRotator::ZeroRotator, SkyParams);
}
