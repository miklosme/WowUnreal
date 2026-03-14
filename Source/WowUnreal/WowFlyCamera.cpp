#include "WowFlyCamera.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "GameFramework/FloatingPawnMovement.h"

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
