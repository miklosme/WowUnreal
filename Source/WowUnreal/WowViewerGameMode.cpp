#include "WowViewerGameMode.h"
#include "WowFlyCamera.h"
#include "WowViewerPlayerController.h"
#include "WowWorldManager.h"
#include "Engine/World.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/SkyLight.h"
#include "Kismet/GameplayStatics.h"

AWowViewerGameMode::AWowViewerGameMode()
{
    DefaultPawnClass = AWowFlyCamera::StaticClass();
    PlayerControllerClass = AWowViewerPlayerController::StaticClass();
}

void AWowViewerGameMode::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (!World) return;

    // Spawn lighting so the scene isn't black
    {
        // Directional light (sun)
        FActorSpawnParameters LightParams;
        LightParams.Name = FName(TEXT("WowSunLight"));
        ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(
            ADirectionalLight::StaticClass(),
            FVector::ZeroVector,
            FRotator(-45.0f, 30.0f, 0.0f),
            LightParams);
        if (Sun)
        {
            Sun->GetLightComponent()->SetIntensity(5.0f);
            Sun->GetLightComponent()->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f));
            // Atmosphere sun light is set automatically for directional lights
            UE_LOG(LogTemp, Log, TEXT("Spawned directional light (sun)"));
        }

        // Sky atmosphere
        AActor* SkyAtmo = World->SpawnActor<AActor>(AActor::StaticClass());
        if (SkyAtmo)
        {
            USkyAtmosphereComponent* Atmo = NewObject<USkyAtmosphereComponent>(SkyAtmo);
            Atmo->RegisterComponent();
            SkyAtmo->AddInstanceComponent(Atmo);
            UE_LOG(LogTemp, Log, TEXT("Spawned sky atmosphere"));
        }

        // Sky light for ambient
        FActorSpawnParameters SkyLightParams;
        SkyLightParams.Name = FName(TEXT("WowSkyLight"));
        ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(
            ASkyLight::StaticClass(),
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SkyLightParams);
        if (SkyLight)
        {
            SkyLight->GetLightComponent()->SetIntensity(1.0f);
            SkyLight->GetLightComponent()->bRealTimeCapture = true;
            UE_LOG(LogTemp, Log, TEXT("Spawned sky light"));
        }
    }

    // Auto-spawn the World Manager
    {
        TArray<AActor*> Found;
        UGameplayStatics::GetAllActorsOfClass(World, AWowWorldManager::StaticClass(), Found);

        if (Found.Num() == 0)
        {
            FActorSpawnParameters Params;
            Params.Name = FName(TEXT("WowWorldManager"));
            AWowWorldManager* Manager = World->SpawnActor<AWowWorldManager>(
                AWowWorldManager::StaticClass(),
                FVector::ZeroVector,
                FRotator::ZeroRotator,
                Params);
            if (Manager)
            {
                UE_LOG(LogTemp, Log, TEXT("Auto-spawned WowWorldManager"));
            }
        }
    }
}
