#include "WowViewerGameMode.h"
#include "WowFlyCamera.h"
#include "WowViewerPlayerController.h"
#include "WowWorldManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

AWowViewerGameMode::AWowViewerGameMode()
{
    DefaultPawnClass = AWowFlyCamera::StaticClass();
    PlayerControllerClass = AWowViewerPlayerController::StaticClass();
}

void AWowViewerGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Auto-spawn the World Manager if not already in level
    UWorld* World = GetWorld();
    if (World)
    {
        // Check if one already exists
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(World, AWowWorldManager::StaticClass(), Found);

        if (Found.Num() == 0)
        {
            FActorSpawnParameters Params;
            Params.Name = FName(TEXT("WowWorldManager"));
            AWowWorldManager* Manager = World->SpawnActor<AWowWorldManager>(AWowWorldManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
            if (Manager)
            {
                UE_LOG(LogTemp, Log, TEXT("Auto-spawned WowWorldManager"));
            }
        }
    }
}
