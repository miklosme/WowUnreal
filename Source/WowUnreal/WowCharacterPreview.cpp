#include "WowCharacterPreview.h"
#include "WowWorldManager.h"
#include "WowCharacterBuilder.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowPreview, Log, All);

// Place preview scene far below the world
const FVector AWowCharacterPreview::PREVIEW_OFFSET = FVector(0.0f, 0.0f, -500000.0f);

AWowCharacterPreview::AWowCharacterPreview()
{
    PrimaryActorTick.bCanEverTick = false;

    // Scene root
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // Scene capture
    CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
    CaptureComponent->SetupAttachment(Root);
    CaptureComponent->CaptureSource = SCS_FinalColorLDR;
    CaptureComponent->bCaptureEveryFrame = false;
    CaptureComponent->bCaptureOnMovement = false;
    CaptureComponent->FOVAngle = 30.0f;
    CaptureComponent->ShowFlags.SetFog(false);
    CaptureComponent->ShowFlags.SetAtmosphere(false);
    CaptureComponent->ShowFlags.SetVolumetricFog(false);

    // Key light (main illumination from front-left)
    KeyLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("KeyLight"));
    KeyLight->SetupAttachment(Root);
    KeyLight->SetRelativeRotation(FRotator(-35.0f, -30.0f, 0.0f));
    KeyLight->Intensity = 4.0f;
    KeyLight->LightColor = FColor(255, 248, 240);

    // Fill light (softer, from front-right)
    FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
    FillLight->SetupAttachment(Root);
    FillLight->SetRelativeLocation(FVector(-200.0f, 150.0f, 120.0f));
    FillLight->Intensity = 6000.0f;
    FillLight->AttenuationRadius = 1000.0f;
    FillLight->LightColor = FColor(240, 240, 255);

    // Rim light (from behind for character silhouette)
    RimLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RimLight"));
    RimLight->SetupAttachment(Root);
    RimLight->SetRelativeLocation(FVector(200.0f, 0.0f, 150.0f));
    RimLight->Intensity = 8000.0f;
    RimLight->AttenuationRadius = 800.0f;
    RimLight->LightColor = FColor(200, 220, 255);
}

void AWowCharacterPreview::BeginPlay()
{
    Super::BeginPlay();
}

void AWowCharacterPreview::Setup(AWowWorldManager* InWorldManager, int32 Width, int32 Height)
{
    WorldManager = InWorldManager;

    // Create render target
    RenderTarget = NewObject<UTextureRenderTarget2D>(this);
    RenderTarget->InitAutoFormat(Width, Height);
    RenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
    RenderTarget->UpdateResourceImmediate(true);

    CaptureComponent->TextureTarget = RenderTarget;

    // Position the entire preview rig far below world
    SetActorLocation(PREVIEW_OFFSET);

    // Default camera position (will be adjusted per-race)
    CaptureComponent->SetRelativeLocation(FVector(-300.0f, 0.0f, 90.0f));
    CaptureComponent->SetRelativeRotation(FRotator(-5.0f, 0.0f, 0.0f));

    UE_LOG(LogWowPreview, Log, TEXT("Character preview initialized (%dx%d)"), Width, Height);
}

void AWowCharacterPreview::ShowCharacter(uint8 RaceId, uint8 GenderId)
{
    ClearCharacter();

    if (!WorldManager || !WorldManager->GetMpqManager() || !WorldManager->GetAssetCache())
    {
        UE_LOG(LogWowPreview, Warning, TEXT("Cannot show character preview — no MPQ/AssetCache"));
        return;
    }

    auto Race = static_cast<FWowCharacterBuilder::ERace>(RaceId);
    auto Gender = static_cast<FWowCharacterBuilder::EGender>(GenderId);

    // Spawn character at the preview location, facing the camera
    FVector CharPos = PREVIEW_OFFSET;
    FRotator CharRot(0.0f, 180.0f, 0.0f); // Face camera

    CurrentCharacterActor = FWowCharacterBuilder::SpawnCharacter(
        GetWorld(), WorldManager->GetMpqManager(), WorldManager->GetAssetCache(),
        Race, Gender, CharPos, CharRot);

    if (CurrentCharacterActor)
    {
        // Adjust camera for race (taller races need further/higher camera)
        PositionCamera(RaceId);

        // Capture with a short delay to let shaders compile
        FTimerHandle CaptureTimer;
        GetWorldTimerManager().SetTimer(CaptureTimer, [this]()
        {
            CaptureNow();
            // Capture again after a moment for shader compilation
            FTimerHandle SecondCapture;
            GetWorldTimerManager().SetTimer(SecondCapture, [this]()
            {
                CaptureNow();
            }, 1.0f, false);
        }, 0.2f, false);

        UE_LOG(LogWowPreview, Log, TEXT("Spawned preview character: Race=%d Gender=%d"), RaceId, GenderId);
    }
}

void AWowCharacterPreview::PositionCamera(uint8 RaceId)
{
    // Adjust camera distance/height based on race model size
    float CamDist = -300.0f;
    float CamHeight = 90.0f;
    float CamPitch = -5.0f;

    switch (RaceId)
    {
    case 6: // Tauren — tallest
        CamDist = -400.0f;
        CamHeight = 120.0f;
        CamPitch = -8.0f;
        break;
    case 4: // Night Elf — tall
    case 11: // Draenei — tall
        CamDist = -340.0f;
        CamHeight = 100.0f;
        break;
    case 7: // Gnome — shortest
        CamDist = -220.0f;
        CamHeight = 50.0f;
        CamPitch = -3.0f;
        break;
    case 3: // Dwarf — short
        CamDist = -250.0f;
        CamHeight = 60.0f;
        break;
    default:
        break;
    }

    CaptureComponent->SetRelativeLocation(FVector(CamDist, 0.0f, CamHeight));
    CaptureComponent->SetRelativeRotation(FRotator(CamPitch, 0.0f, 0.0f));
}

void AWowCharacterPreview::ClearCharacter()
{
    if (CurrentCharacterActor && CurrentCharacterActor->IsValidLowLevel())
    {
        CurrentCharacterActor->Destroy();
        CurrentCharacterActor = nullptr;
    }
}

void AWowCharacterPreview::CaptureNow()
{
    if (CaptureComponent && RenderTarget)
    {
        CaptureComponent->CaptureScene();
    }
}
