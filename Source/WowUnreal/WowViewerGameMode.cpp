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
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"

AWowViewerGameMode::AWowViewerGameMode()
{
    DefaultPawnClass = AWowFlyCamera::StaticClass();
    PlayerControllerClass = AWowGameplayController::StaticClass();
    HUDClass = AWowDebugHUD::StaticClass();
}

void AWowViewerGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Force fixed pre-exposure via CVar as early as possible to prevent
    // FinalPreExposure=0.0 ensure failure on first rendered frames
    if (IConsoleVariable* PreExpCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride")))
    {
        PreExpCVar->Set(1.0f);
        UE_LOG(LogTemp, Log, TEXT("Set r.EyeAdaptation.PreExposureOverride=1.0 (fixed pre-exposure)"));
    }

    UWorld* World = GetWorld();
    if (!World) return;

    // Spawn an unbound PostProcessVolume with fixed manual exposure to prevent black screen.
    // The key fix is r.EyeAdaptation.PreExposureOverride=1.0 in DefaultEngine.ini which
    // forces FinalPreExposure=1.0 bypassing the eye adaptation computation entirely.
    // This PPV provides belt-and-suspenders protection via manual exposure mode.
    {
        APostProcessVolume* PPV = World->SpawnActor<APostProcessVolume>();
        if (PPV)
        {
            PPV->bUnbound = true;
            PPV->Settings.bOverride_AutoExposureMethod = true;
            PPV->Settings.AutoExposureMethod = AEM_Manual;
            PPV->Settings.bOverride_AutoExposureBias = true;
            PPV->Settings.AutoExposureBias = 0.0f; // EV100=0 → exposure multiplier 1.0
            PPV->Settings.bOverride_AutoExposureMinBrightness = true;
            PPV->Settings.AutoExposureMinBrightness = 1.0f;
            PPV->Settings.bOverride_AutoExposureMaxBrightness = true;
            PPV->Settings.AutoExposureMaxBrightness = 1.0f;
            UE_LOG(LogTemp, Log, TEXT("Spawned global PostProcessVolume with fixed manual exposure (EV0, PreExposureOverride=1.0)"));
        }
    }

    // Spawn sky manager (handles sun, moon, sky atmosphere, fog, time-of-day)
    {
        FActorSpawnParameters SkyParams;
        SkyParams.Name = FName(TEXT("WowSkyManager"));
        AWowSkyManager* SkyMgr = World->SpawnActor<AWowSkyManager>(
            AWowSkyManager::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SkyParams);
        if (SkyMgr)
        {
            UE_LOG(LogTemp, Log, TEXT("Spawned WowSkyManager (time-of-day, sun/moon, fog)"));
        }
    }

    // Auto-spawn the World Manager
    AWowWorldManager* WorldManager = nullptr;
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(World, AWowWorldManager::StaticClass(), Found);

        if (Found.Num() == 0)
        {
            FActorSpawnParameters Params;
            Params.Name = FName(TEXT("WowWorldManager"));
            WorldManager = World->SpawnActor<AWowWorldManager>(
                AWowWorldManager::StaticClass(),
                FVector::ZeroVector,
                FRotator::ZeroRotator,
                Params);
            if (WorldManager)
            {
                UE_LOG(LogTemp, Log, TEXT("Auto-spawned WowWorldManager"));
            }
        }
        else
        {
            WorldManager = Cast<AWowWorldManager>(Found[0]);
        }
    }

    // Load WoW UI (FrameXML + addons) once MpqManager is available
    if (WorldManager && WorldManager->GetMpqManager())
    {
        UGameInstance* GI = GetGameInstance();
        if (GI)
        {
            UWowUIManager* UIManager = GI->GetSubsystem<UWowUIManager>();
            if (UIManager)
            {
                UIManager->LoadUI(WorldManager->GetMpqManager());
            }
        }
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UWowAutoLogin* AutoLogin = GI->GetSubsystem<UWowAutoLogin>())
        {
            AutoLogin->TryAutoLogin();

            if (AWowGameplayController* GameplayController = Cast<AWowGameplayController>(UGameplayStatics::GetPlayerController(this, 0)))
            {
                if (UWowConnectionManager* ConnectionManager = AutoLogin->GetConnectionManager())
                {
                    GameplayController->ConnectionManager = ConnectionManager;
                    GameplayController->BindEntityEvents();
                    UE_LOG(LogTemp, Log, TEXT("Bound gameplay controller to autologin connection manager"));
                }
            }
        }
    }
}
