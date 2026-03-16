#include "WowModelViewerGameMode.h"
#include "WowModelViewerWidget.h"
#include "WowOrbitCamera.h"
#include "WowWorldManager.h"
#include "WowCharacterBuilder.h"
#include "WowEquipmentManager.h"
#include "WowDebugHUD.h"
#include "Formats/Dbc/DbcStore.h"
#include "Engine/World.h"
#include "Engine/PointLight.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"

DEFINE_LOG_CATEGORY_STATIC(LogModelViewer, Log, All);

AWowModelViewerGameMode::AWowModelViewerGameMode()
{
    DefaultPawnClass = AWowOrbitCamera::StaticClass();
    HUDClass = AWowDebugHUD::StaticClass();
}

void AWowModelViewerGameMode::SetupTestScene(UWorld* World, AWowWorldManager* WorldManager)
{
    UE_LOG(LogModelViewer, Log, TEXT("Setting up Model Viewer scene"));

    WorldMgr = WorldManager;

    // Disable terrain loading — we only need MPQ/DBC access
    if (WorldManager)
    {
        WorldManager->LoadRadius = 0;
        WorldManager->UnloadRadius = 0;
        WorldManager->WdlRadius = 0;
        WorldManager->WdlUnloadRadius = 0;
        WorldManager->Lod1Radius = 0;
        WorldManager->MaxActiveDoodads = 0;
        WorldManager->MaxActiveWmoGroups = 0;
        // Keep tick enabled for auto-screenshot but terrain is disabled by LoadRadius=0
        // WorldManager->SetActorTickEnabled(false);
    }

    // Scene setup: sky, lights, ground
    SpawnSkyManager(World);
    SpawnDirectionalLight(World);
    SpawnGroundPlane(World, FVector(0.0f, 0.0f, -5.0f), 30.0f);

    // Front key light — neutral white
    if (APointLight* FillLight = World->SpawnActor<APointLight>(
        APointLight::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(-400.0f, -200.0f, 300.0f))))
    {
        if (UPointLightComponent* LC = Cast<UPointLightComponent>(FillLight->GetLightComponent()))
        {
            LC->SetIntensity(8000.0f);
            LC->SetAttenuationRadius(2000.0f);
            LC->SetLightColor(FLinearColor(1.0f, 1.0f, 1.0f));
        }
    }

    // Back rim light — slight blue
    if (APointLight* BackLight = World->SpawnActor<APointLight>(
        APointLight::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(400.0f, 200.0f, 300.0f))))
    {
        if (UPointLightComponent* LC = Cast<UPointLightComponent>(BackLight->GetLightComponent()))
        {
            LC->SetIntensity(5000.0f);
            LC->SetAttenuationRadius(2000.0f);
            LC->SetLightColor(FLinearColor(0.9f, 0.95f, 1.0f));
        }
    }

    // Side fill — neutral
    if (APointLight* SideLight = World->SpawnActor<APointLight>(
        APointLight::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(0.0f, -400.0f, 200.0f))))
    {
        if (UPointLightComponent* LC = Cast<UPointLightComponent>(SideLight->GetLightComponent()))
        {
            LC->SetIntensity(4000.0f);
            LC->SetAttenuationRadius(2000.0f);
            LC->SetLightColor(FLinearColor(1.0f, 1.0f, 1.0f));
        }
    }

    // Create UI after a brief delay so the viewport is ready
    FTimerHandle UITimer;
    World->GetTimerManager().SetTimer(UITimer, [this]()
    {
        CreateUI();
        RespawnCharacter();

        // Position orbit camera to look at character waist height
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            if (AWowOrbitCamera* OrbitCam = Cast<AWowOrbitCamera>(PC->GetPawn()))
            {
                OrbitCam->SetFocalPoint(FVector(0.0f, 0.0f, 80.0f));
            }
        }
    }, 0.2f, false);
}

void AWowModelViewerGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ViewerWidgetContainer.IsValid())
    {
        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->RemoveViewportWidgetContent(ViewerWidgetContainer.ToSharedRef());
        }
        ViewerWidgetContainer.Reset();
        ViewerWidget.Reset();
    }

    Super::EndPlay(EndPlayReason);
}

void AWowModelViewerGameMode::CreateUI()
{
    if (!GEngine || !GEngine->GameViewport) return;

    ViewerWidget = SNew(SWowModelViewerWidget);

    ViewerWidget->OnCharacterChanged.BindLambda([this]()
    {
        RespawnCharacter();
    });

    ViewerWidget->OnCreatureSpawn.BindLambda([this](uint32 DisplayId)
    {
        SpawnCreature(DisplayId);
    });

    ViewerWidgetContainer =
        SNew(SOverlay)
        + SOverlay::Slot()
        .HAlign(HAlign_Left)
        .VAlign(VAlign_Fill)
        .Padding(FMargin(10, 10, 0, 10))
        [
            ViewerWidget.ToSharedRef()
        ];

    GEngine->GameViewport->AddViewportWidgetContent(ViewerWidgetContainer.ToSharedRef(), 10);

    UE_LOG(LogModelViewer, Log, TEXT("Model Viewer UI created"));
}

void AWowModelViewerGameMode::RespawnCharacter()
{
    if (!WorldMgr || !WorldMgr->GetMpqManager() || !WorldMgr->GetAssetCache())
    {
        if (ViewerWidget.IsValid())
            ViewerWidget->SetStatusText(TEXT("Waiting for MPQ data..."));
        return;
    }

    DestroyCurrentModel();

    FMpqManager* Mpq = WorldMgr->GetMpqManager();
    FWowAssetCache* Cache = WorldMgr->GetAssetCache();

    FWowCharacterBuilder::FCharacterParams Params;
    Params.Race = ViewerWidget->GetSelectedRace();
    Params.Gender = ViewerWidget->GetSelectedGender();
    Params.Customization.RaceId = static_cast<uint32>(Params.Race);
    Params.Customization.Gender = static_cast<uint32>(Params.Gender);
    Params.Customization.SkinColor = ViewerWidget->GetSkinColor();
    Params.Customization.FaceVariation = ViewerWidget->GetFaceVariation();
    Params.Customization.HairStyle = ViewerWidget->GetHairStyle();
    Params.Customization.HairColor = ViewerWidget->GetHairColor();
    Params.Customization.FacialHairStyle = ViewerWidget->GetFacialHairStyle();

    // Equipment — manual weapon ID
    bool bDrawn = ViewerWidget->GetWeaponsDrawn();
    uint32 WeaponId = ViewerWidget->GetWeaponDisplayId();
    if (WeaponId > 0)
    {
        FWowCharacterBuilder::FEquipmentSlot Slot;
        Slot.ItemDisplayId = WeaponId;
        Slot.AttachPoint = bDrawn ? FWowEquipmentManager::EAttachmentPoint::RightHand
                                  : FWowEquipmentManager::EAttachmentPoint::Back;
        Params.Equipment.Add(Slot);
    }

    // Starter gear: find first available equipment for each attachment slot
    if (ViewerWidget->GetShowStarterGear())
    {
        struct FSlotSearch
        {
            FWowEquipmentManager::EAttachmentPoint Point;
            uint32 FoundId = 0;
        };
        TArray<FSlotSearch> Slots = {
            {FWowEquipmentManager::EAttachmentPoint::RightHand},
            {FWowEquipmentManager::EAttachmentPoint::LeftHand},
            {FWowEquipmentManager::EAttachmentPoint::Helmet},
            {FWowEquipmentManager::EAttachmentPoint::LeftShoulder},
            {FWowEquipmentManager::EAttachmentPoint::Back},
            {FWowEquipmentManager::EAttachmentPoint::Shield},
        };

        FString ResolvedPath;
        for (const FItemDisplayInfoDbcEntry& Entry : FDbcStore::Get().ItemDisplayInfo().GetAll())
        {
            bool bAllFound = true;
            for (auto& S : Slots)
            {
                if (S.FoundId == 0)
                {
                    if (FWowEquipmentManager::ResolveEquipmentPaths(Mpq, Entry.ID, S.Point, ResolvedPath))
                    {
                        S.FoundId = Entry.ID;
                    }
                    else
                    {
                        bAllFound = false;
                    }
                }
            }
            if (bAllFound) break;
        }

        auto AddEquip = [&Params](uint32 Id, FWowEquipmentManager::EAttachmentPoint Pt)
        {
            if (Id == 0) return;
            FWowCharacterBuilder::FEquipmentSlot Slot;
            Slot.ItemDisplayId = Id;
            Slot.AttachPoint = Pt;
            Params.Equipment.Add(Slot);
        };

        for (const auto& S : Slots)
        {
            auto Point = S.Point;
            // Swap weapon attachment points based on draw/sheathe toggle
            if (!bDrawn)
            {
                if (Point == FWowEquipmentManager::EAttachmentPoint::RightHand)
                    Point = FWowEquipmentManager::EAttachmentPoint::Back; // Sheathed on back
                else if (Point == FWowEquipmentManager::EAttachmentPoint::LeftHand)
                    Point = FWowEquipmentManager::EAttachmentPoint::Shield; // Shield on back
            }
            AddEquip(S.FoundId, Point);
            // Shoulders: add both sides
            if (S.Point == FWowEquipmentManager::EAttachmentPoint::LeftShoulder && S.FoundId > 0)
            {
                AddEquip(S.FoundId, FWowEquipmentManager::EAttachmentPoint::RightShoulder);
            }
        }

        UE_LOG(LogModelViewer, Log, TEXT("Starter gear equipped: %d slots"), Params.Equipment.Num());
    }

    const FRotator FacingCamera(0.0f, -90.0f, 0.0f);
    CurrentModelActor = FWowCharacterBuilder::SpawnCharacterWithEquipment(
        GetWorld(), Mpq, Cache, Params, FVector::ZeroVector, FacingCamera);

    if (CurrentModelActor)
    {
        FString RaceName = FString::Printf(TEXT("%s %s"),
            *FWowCharacterBuilder::GetCharacterModelPath(Params.Race, Params.Gender).Left(30),
            Params.Gender == FWowCharacterBuilder::EGender::Male ? TEXT("Male") : TEXT("Female"));
        ViewerWidget->SetStatusText(FString::Printf(TEXT("Spawned: Race %d %s"),
            static_cast<int32>(Params.Race),
            Params.Gender == FWowCharacterBuilder::EGender::Male ? TEXT("Male") : TEXT("Female")));
    }
    else
    {
        ViewerWidget->SetStatusText(TEXT("Failed to spawn character"));
    }
}

void AWowModelViewerGameMode::SpawnCreature(uint32 DisplayId)
{
    if (DisplayId == 0)
    {
        if (ViewerWidget.IsValid())
            ViewerWidget->SetStatusText(TEXT("Enter a display ID"));
        return;
    }

    if (!WorldMgr || !WorldMgr->GetMpqManager() || !WorldMgr->GetAssetCache())
    {
        if (ViewerWidget.IsValid())
            ViewerWidget->SetStatusText(TEXT("Waiting for MPQ data..."));
        return;
    }

    DestroyCurrentModel();

    const FRotator FacingCamera(0.0f, -90.0f, 0.0f);
    CurrentModelActor = FWowCharacterBuilder::SpawnCreatureByDisplayId(
        GetWorld(), WorldMgr->GetMpqManager(), WorldMgr->GetAssetCache(),
        DisplayId, FVector::ZeroVector, FacingCamera);

    if (CurrentModelActor)
    {
        ViewerWidget->SetStatusText(FString::Printf(TEXT("Creature DisplayId=%d"), DisplayId));
    }
    else
    {
        ViewerWidget->SetStatusText(FString::Printf(TEXT("Failed: DisplayId=%d not found"), DisplayId));
    }
}

void AWowModelViewerGameMode::DestroyCurrentModel()
{
    if (CurrentModelActor && CurrentModelActor->IsValidLowLevel())
    {
        CurrentModelActor->Destroy();
        CurrentModelActor = nullptr;
    }
}
