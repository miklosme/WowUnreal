#include "WowViewerGameMode.h"
#include "WowPlayerCharacter.h"
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

AWowViewerGameMode::AWowViewerGameMode()
{
    DefaultPawnClass = AWowPlayerCharacter::StaticClass();
    PlayerControllerClass = AWowGameplayController::StaticClass();
    HUDClass = AWowDebugHUD::StaticClass();
}

void AWowViewerGameMode::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (!World) return;

    // Spawn an unbound PostProcessVolume with fixed exposure to prevent black screen
    // (FinalPreExposure > 0.0f ensure from eye adaptation computing zero)
    {
        APostProcessVolume* PPV = World->SpawnActor<APostProcessVolume>();
        if (PPV)
        {
            PPV->bUnbound = true;
            PPV->Settings.bOverride_AutoExposureMethod = true;
            PPV->Settings.AutoExposureMethod = AEM_Manual;
            PPV->Settings.bOverride_AutoExposureBias = true;
            PPV->Settings.AutoExposureBias = 10.0f;
            UE_LOG(LogTemp, Log, TEXT("Spawned global PostProcessVolume with fixed manual exposure"));
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
