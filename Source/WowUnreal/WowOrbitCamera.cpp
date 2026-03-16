#include "WowOrbitCamera.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Engine/World.h"

AWowOrbitCamera::AWowOrbitCamera()
{
    PrimaryActorTick.bCanEverTick = true;
    SetActorEnableCollision(false);
}

void AWowOrbitCamera::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;

        // Ensure MouseWheelAxis is mapped
        if (PC->PlayerInput)
        {
            FInputAxisKeyMapping WheelUp(TEXT("MouseWheelAxis"), EKeys::MouseScrollUp, 1.0f);
            FInputAxisKeyMapping WheelDown(TEXT("MouseWheelAxis"), EKeys::MouseScrollDown, -1.0f);
            PC->PlayerInput->AxisMappings.AddUnique(WheelUp);
            PC->PlayerInput->AxisMappings.AddUnique(WheelDown);
            PC->PlayerInput->ForceRebuildingKeyMaps(false);
        }
    }

    UpdateCameraTransform();

    // Auto-screenshot support (same as WowFlyCamera)
    FString ScreenshotPath;
    if (FParse::Value(FCommandLine::Get(), TEXT("autoscreenshot="), ScreenshotPath))
    {
        float Delay = 15.0f;
        FParse::Value(FCommandLine::Get(), TEXT("autoscreenshotdelay="), Delay);
        FTimerHandle TimerHandle;
        GetWorldTimerManager().SetTimer(TimerHandle, [this, ScreenshotPath]()
        {
            if (APlayerController* PC = Cast<APlayerController>(GetController()))
            {
                PC->ConsoleCommand(TEXT("Shot"));
            }
        }, Delay, false);
    }
}

void AWowOrbitCamera::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    float MouseX, MouseY;
    PC->GetMousePosition(MouseX, MouseY);
    FVector2D CurrentMouse(MouseX, MouseY);

    bool bLMB = PC->IsInputKeyDown(EKeys::LeftMouseButton);
    bool bMMB = PC->IsInputKeyDown(EKeys::MiddleMouseButton);

    FVector2D Delta = CurrentMouse - LastMousePos;

    // LMB drag = orbit rotate
    if (bLMB && bLeftMouseDown)
    {
        OrbitYaw += Delta.X * RotateSpeed;
        OrbitPitch = FMath::Clamp(OrbitPitch - Delta.Y * RotateSpeed, -89.0f, 89.0f);
    }

    // MMB drag = pan focal point
    if (bMMB && bMiddleMouseDown)
    {
        float YawRad = FMath::DegreesToRadians(OrbitYaw);
        FVector Right(-FMath::Sin(YawRad), FMath::Cos(YawRad), 0.0f);
        FVector Up(0.0f, 0.0f, 1.0f);
        FocalPoint -= Right * Delta.X * PanSpeed + Up * Delta.Y * PanSpeed;
    }

    bLeftMouseDown = bLMB;
    bMiddleMouseDown = bMMB;
    LastMousePos = CurrentMouse;

    UpdateCameraTransform();
}

void AWowOrbitCamera::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    FInputAxisBinding& WheelBinding = PlayerInputComponent->BindAxis("MouseWheelAxis");
    WheelBinding.AxisDelegate.GetDelegateForManualSet().BindLambda([this](float Value)
    {
        if (FMath::Abs(Value) > KINDA_SMALL_NUMBER)
        {
            OrbitDistance = FMath::Clamp(OrbitDistance - Value * ZoomSpeed, MinDistance, MaxDistance);
        }
    });
}

void AWowOrbitCamera::SetFocalPoint(const FVector& InFocalPoint)
{
    FocalPoint = InFocalPoint;
}

void AWowOrbitCamera::UpdateCameraTransform()
{
    float PitchRad = FMath::DegreesToRadians(OrbitPitch);
    float YawRad = FMath::DegreesToRadians(OrbitYaw);

    FVector Offset(
        OrbitDistance * FMath::Cos(PitchRad) * FMath::Cos(YawRad),
        OrbitDistance * FMath::Cos(PitchRad) * FMath::Sin(YawRad),
        OrbitDistance * FMath::Sin(PitchRad));

    FVector CamPos = FocalPoint + Offset;
    SetActorLocation(CamPos);

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        FRotator LookRot = (FocalPoint - CamPos).Rotation();
        PC->SetControlRotation(LookRot);
    }
}
