#include "WowViewerGameMode.h"
#include "WowFlyCamera.h"
#include "WowGameplayController.h"
#include "WowDebugHUD.h"
#include "WowAutoLogin.h"
#include "WowWorldManager.h"
#include "WowSkyManager.h"
#include "WowUIManager.h"
#include "WowConnectionManager.h"
#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "WowCharacterBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowGameMode, Log, All);

AWowViewerGameMode::AWowViewerGameMode()
{
    DefaultPawnClass = AWowFlyCamera::StaticClass();
    PlayerControllerClass = AWowGameplayController::StaticClass();
    HUDClass = AWowDebugHUD::StaticClass();
}

void AWowViewerGameMode::SetupPostProcess(UWorld* World)
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

AWowWorldManager* AWowViewerGameMode::SpawnWorldManager(UWorld* World)
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
        UE_LOG(LogWowGameMode, Log, TEXT("Spawned WowWorldManager"));
    }
    return WM;
}

void AWowViewerGameMode::SpawnGroundPlane(UWorld* World, const FVector& Center, float Size)
{
    // Spawn a flat static mesh plane for test scenes
    UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (!PlaneMesh) return;

    AStaticMeshActor* PlaneActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, Center));
    if (!PlaneActor) return;

    PlaneActor->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
    PlaneActor->SetActorScale3D(FVector(Size, Size, 1.0f));
    PlaneActor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PlaneActor->GetStaticMeshComponent()->SetCollisionObjectType(ECC_WorldStatic);
    PlaneActor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);

    UE_LOG(LogWowGameMode, Log, TEXT("Spawned ground plane at %s (scale %.0f)"), *Center.ToString(), Size);
}

void AWowViewerGameMode::SpawnDirectionalLight(UWorld* World)
{
    ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(
        ADirectionalLight::StaticClass(), FTransform(FRotator(-45.0f, 30.0f, 0.0f), FVector::ZeroVector));
    if (Sun)
    {
        Sun->GetLightComponent()->SetIntensity(3.14159f);
        Sun->GetLightComponent()->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f));
    }
}

void AWowViewerGameMode::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (!World) return;

    // Parse test scene selector
    FString TestScene;
    FParse::Value(FCommandLine::Get(), TEXT("-testscene="), TestScene);
    TestScene = TestScene.ToLower();

    // Post-process is always needed
    SetupPostProcess(World);

    if (TestScene == TEXT("character"))
    {
        SetupCharacterTestScene(World);
    }
    else if (TestScene == TEXT("terrain"))
    {
        SetupTerrainTestScene(World);
    }
    else if (TestScene == TEXT("wmo"))
    {
        SetupWmoTestScene(World);
    }
    else if (TestScene == TEXT("ui"))
    {
        SetupUITestScene(World);
    }
    else
    {
        // Default: full world
        SetupDefaultScene(World);
    }
}

void AWowViewerGameMode::SetupDefaultScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("Starting default (full world) scene"));

    // Spawn sky
    {
        FActorSpawnParameters SkyParams;
        SkyParams.Name = FName(TEXT("WowSkyManager"));
        World->SpawnActor<AWowSkyManager>(AWowSkyManager::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SkyParams);
    }

    // World manager (full streaming)
    AWowWorldManager* WorldManager = SpawnWorldManager(World);

    // Load UI
    if (WorldManager && WorldManager->GetMpqManager())
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UWowUIManager* UIManager = GI->GetSubsystem<UWowUIManager>())
            {
                UIManager->LoadUI(WorldManager->GetMpqManager());
            }
        }
    }

    // Auto-login + gameplay controller binding
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UWowAutoLogin* AutoLogin = GI->GetSubsystem<UWowAutoLogin>())
        {
            AutoLogin->TryAutoLogin();
            if (AWowGameplayController* GPC = Cast<AWowGameplayController>(UGameplayStatics::GetPlayerController(this, 0)))
            {
                if (UWowConnectionManager* CM = AutoLogin->GetConnectionManager())
                {
                    GPC->ConnectionManager = CM;
                    GPC->BindEntityEvents();
                }
            }
        }
    }
}

void AWowViewerGameMode::SetupCharacterTestScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("Starting character/animation test scene"));

    SpawnDirectionalLight(World);
    SpawnGroundPlane(World, FVector::ZeroVector, 100.0f);

    // Teleport player just above the ground plane
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC && PC->GetPawn())
    {
        PC->GetPawn()->SetActorLocation(FVector(0.0f, 0.0f, 200.0f));
        PC->SetControlRotation(FRotator(-15.0f, 0.0f, 0.0f));
    }

    // Spawn world manager for MPQ access (terrain loading skipped via -testscene check in WM::BeginPlay)
    AWowWorldManager* WM = SpawnWorldManager(World);

    // Spawn test character models — Human Male and Female for visual verification
    if (WM && WM->GetMpqManager() && WM->GetAssetCache())
    {
        FWowCharacterBuilder::SpawnCharacter(World, WM->GetMpqManager(), WM->GetAssetCache(),
            FWowCharacterBuilder::ERace::Human, FWowCharacterBuilder::EGender::Male,
            FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));

        FWowCharacterBuilder::SpawnCharacter(World, WM->GetMpqManager(), WM->GetAssetCache(),
            FWowCharacterBuilder::ERace::Human, FWowCharacterBuilder::EGender::Female,
            FVector(0.0f, 300.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));

        // Spawn an Orc for variety
        FWowCharacterBuilder::SpawnCharacter(World, WM->GetMpqManager(), WM->GetAssetCache(),
            FWowCharacterBuilder::ERace::Orc, FWowCharacterBuilder::EGender::Male,
            FVector(0.0f, -300.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
    }
}

void AWowViewerGameMode::SetupTerrainTestScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("Starting single-tile terrain test scene"));

    // Spawn sky for proper lighting
    {
        FActorSpawnParameters SkyParams;
        SkyParams.Name = FName(TEXT("WowSkyManager"));
        World->SpawnActor<AWowSkyManager>(AWowSkyManager::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SkyParams);
    }

    // Spawn world manager — loads 3x3 grid then streaming is disabled via -testscene check
    SpawnWorldManager(World);
}

void AWowViewerGameMode::SetupWmoTestScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("Starting WMO test scene"));

    SpawnDirectionalLight(World);

    // Spawn world manager — loads 3x3 grid with WMOs, streaming disabled via -testscene check
    SpawnWorldManager(World);
}

void AWowViewerGameMode::SetupUITestScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("Starting UI test scene"));

    SpawnDirectionalLight(World);
    SpawnGroundPlane(World, FVector::ZeroVector, 10.0f);

    // Teleport player
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC && PC->GetPawn())
    {
        PC->GetPawn()->SetActorLocation(FVector(0.0f, 0.0f, 200.0f));
    }

    // Spawn world manager for MPQ access (terrain loading skipped via -testscene check)
    AWowWorldManager* WM = SpawnWorldManager(World);

    // Load UI
    if (WM && WM->GetMpqManager())
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UWowUIManager* UIManager = GI->GetSubsystem<UWowUIManager>())
            {
                UIManager->LoadUI(WM->GetMpqManager());
            }
        }
    }
}
