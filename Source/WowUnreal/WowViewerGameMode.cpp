#include "WowViewerGameMode.h"
#include "WowPlayerCharacter.h"
#include "WowGameplayController.h"
#include "WowDebugHUD.h"
#include "WowWorldManager.h"
#include "WowSkyManager.h"
#include "WowUIManager.h"
#include "Engine/World.h"
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
}
