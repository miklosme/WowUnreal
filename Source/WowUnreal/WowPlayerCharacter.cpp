#include "WowPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

AWowPlayerCharacter::AWowPlayerCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Capsule defaults (roughly human-sized)
    GetCapsuleComponent()->InitCapsuleSize(35.0f, 88.0f);

    // Don't rotate character with controller — camera orbits independently
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    // Movement
    UCharacterMovementComponent* Mov = GetCharacterMovement();
    Mov->bOrientRotationToMovement = true;
    Mov->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    Mov->MaxWalkSpeed = RunSpeed;
    Mov->MaxWalkSpeedCrouched = RunSpeed * WalkSpeedFactor;
    Mov->JumpZVelocity = 600.0f;
    Mov->AirControl = 0.2f;
    Mov->GravityScale = 1.5f;
    Mov->BrakingDecelerationWalking = 2000.0f;
    Mov->MaxStepHeight = 45.0f;
    Mov->SetWalkableFloorAngle(50.0f);

    // Spring arm (camera boom)
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(RootComponent);
    CameraBoom->TargetArmLength = 800.0f; // ~8 yards default
    CameraBoom->bUsePawnControlRotation = false;
    CameraBoom->bDoCollisionTest = true;
    CameraBoom->ProbeSize = 12.0f;
    CameraBoom->ProbeChannel = ECC_Camera;
    CameraBoom->bInheritPitch = false;
    CameraBoom->bInheritYaw = false;
    CameraBoom->bInheritRoll = false;

    // Camera
    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;

    // No mesh yet — character rendering is a later phase
}

void AWowPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Add input mapping context
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (GameplayContext) Sub->AddMappingContext(GameplayContext, 0);
        }
    }

    // Initialize camera rotation
    CameraBoom->SetWorldRotation(FRotator(CameraPitch, CameraYaw, 0.0f));
}

void AWowPlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Smoothly interpolate camera boom to target rotation
    FRotator Target(CameraPitch, CameraYaw, 0.0f);
    FRotator Current = CameraBoom->GetComponentRotation();
    CameraBoom->SetWorldRotation(FMath::RInterpTo(Current, Target, DeltaTime, 15.0f));
}

void AWowPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EI = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction) EI->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AWowPlayerCharacter::OnMove);
        if (LookAction) EI->BindAction(LookAction, ETriggerEvent::Triggered, this, &AWowPlayerCharacter::OnLook);
        if (JumpAction)
        {
            EI->BindAction(JumpAction, ETriggerEvent::Started, this, &AWowPlayerCharacter::OnJumpPressed);
            EI->BindAction(JumpAction, ETriggerEvent::Completed, this, &AWowPlayerCharacter::OnJumpReleased);
        }
        if (ZoomAction) EI->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AWowPlayerCharacter::OnZoom);
    }
}

void AWowPlayerCharacter::OnMove(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();
    if (Input.IsNearlyZero()) return;

    // Movement relative to camera facing
    const FRotator CameraRot(0.0f, CameraYaw, 0.0f);
    const FVector Forward = FRotationMatrix(CameraRot).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(CameraRot).GetUnitAxis(EAxis::Y);

    AddMovementInput(Forward, Input.Y); // W/S
    AddMovementInput(Right, Input.X);   // A/D
}

void AWowPlayerCharacter::OnLook(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();

    CameraYaw += Input.X * 1.0f;
    CameraPitch = FMath::Clamp(CameraPitch + Input.Y * 1.0f, -80.0f, 10.0f);
}

void AWowPlayerCharacter::OnJumpPressed()
{
    Jump();
}

void AWowPlayerCharacter::OnJumpReleased()
{
    StopJumping();
}

void AWowPlayerCharacter::OnZoom(const FInputActionValue& Value)
{
    float Delta = Value.Get<float>();
    float Current = CameraBoom->TargetArmLength;
    CameraBoom->TargetArmLength = FMath::Clamp(Current - Delta * 50.0f, MinCameraDistance, MaxCameraDistance);
}

void AWowPlayerCharacter::ApplyServerSpeeds(float ServerRunSpeed, float ServerWalkSpeed)
{
    RunSpeed = ServerRunSpeed * 100.0f; // WoW units → cm
    WalkSpeedFactor = ServerWalkSpeed / ServerRunSpeed;

    if (UCharacterMovementComponent* Mov = GetCharacterMovement())
    {
        Mov->MaxWalkSpeed = bIsWalking ? RunSpeed * WalkSpeedFactor : RunSpeed;
    }
}
