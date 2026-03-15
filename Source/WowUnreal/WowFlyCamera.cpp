#include "WowFlyCamera.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Coord/WowCoordinate.h"
#include "Misc/FileHelper.h"
#include "ImageUtils.h"

AWowFlyCamera::AWowFlyCamera()
{
    PrimaryActorTick.bCanEverTick = true;
    SetActorEnableCollision(false);
}

void AWowFlyCamera::BeginPlay()
{
    Super::BeginPlay();
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (FlyCameraContext) Sub->AddMappingContext(FlyCameraContext, 0);
        }
    }
    if (UFloatingPawnMovement* Mov = Cast<UFloatingPawnMovement>(GetMovementComponent()))
    {
        Mov->MaxSpeed = FlySpeed;
        Mov->Acceleration = FlySpeed * 4.0f;
        Mov->Deceleration = FlySpeed * 4.0f;
    }

    // If -startpos, teleport to Northshire Abbey at safe altitude
    if (FString(FCommandLine::Get()).Contains(TEXT("startpos")))
    {
        // Northshire Abbey: WoW (-8949, -132, 84) → 15m above terrain
        FVector NorthshirePos = FWowCoordinate::WowToUE(-8949.0f, -132.0f, 84.0f);
        NorthshirePos.Z += 1500.0f; // 15m above ground for overview with visible objects
        SetActorLocation(NorthshirePos);
        UE_LOG(LogTemp, Warning, TEXT("FlyCamera teleported to Northshire: %s"), *NorthshirePos.ToString());
    }

    // Start looking down at the terrain
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->SetControlRotation(FRotator(-15.0f, 0.0f, 0.0f));
    }

    // Auto-screenshot after delay if requested
    FString ScreenshotPath;
    if (FParse::Value(FCommandLine::Get(), TEXT("autoscreenshot="), ScreenshotPath))
    {
        float Delay = 15.0f;
        FParse::Value(FCommandLine::Get(), TEXT("autoscreenshotdelay="), Delay);
        FTimerHandle TimerHandle;
        GetWorldTimerManager().SetTimer(TimerHandle, [this, ScreenshotPath]()
        {
            FString Cmd = FString::Printf(TEXT("HighResShot 1 filename=\"%s\""), *ScreenshotPath);
            if (APlayerController* PC = Cast<APlayerController>(GetController()))
            {
                PC->ConsoleCommand(TEXT("Shot"));
            }
            UE_LOG(LogTemp, Warning, TEXT("Auto-screenshot triggered: %s"), *ScreenshotPath);
        }, Delay, false);
    }
}

void AWowFlyCamera::Tick(float DeltaTime) { Super::Tick(DeltaTime); }

void AWowFlyCamera::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    if (UEnhancedInputComponent* EI = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction) EI->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWowFlyCamera::Move);
        if (LookAction) EI->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWowFlyCamera::Look);
        if (SpeedAction) EI->BindAction(SpeedAction, ETriggerEvent::Triggered, this, &AWowFlyCamera::AdjustSpeed);
    }
}

void AWowFlyCamera::Move(const FInputActionValue& Value)
{
    const FVector Input = Value.Get<FVector>();
    const FRotator Rot = GetControlRotation();
    const FVector Fwd = FRotationMatrix(Rot).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(Rot).GetUnitAxis(EAxis::Y);
    float Mult = bIsSprinting ? SprintMultiplier : 1.0f;
    AddMovementInput(Fwd, Input.X * Mult);
    AddMovementInput(Right, Input.Y * Mult);
    AddMovementInput(FVector::UpVector, Input.Z * Mult);
}

void AWowFlyCamera::Look(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->AddYawInput(Input.X);
        PC->AddPitchInput(Input.Y);
    }
}

void AWowFlyCamera::AdjustSpeed(const FInputActionValue& Value)
{
    FlySpeed = FMath::Clamp(FlySpeed * (1.0f + Value.Get<float>() * 0.1f), MinSpeed, MaxSpeed);
    if (UFloatingPawnMovement* Mov = Cast<UFloatingPawnMovement>(GetMovementComponent()))
    {
        Mov->MaxSpeed = FlySpeed;
        Mov->Acceleration = FlySpeed * 4.0f;
        Mov->Deceleration = FlySpeed * 4.0f;
    }
}
