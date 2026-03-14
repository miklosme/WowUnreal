#include "WowPlayerCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "InputModifiers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowPlayerChar, Log, All);

static UInputAction* CreateInputAction(UObject* Outer, const TCHAR* Name, EInputActionValueType ValueType)
{
	UInputAction* Action = NewObject<UInputAction>(Outer, Name);
	Action->ValueType = ValueType;
	return Action;
}

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
	Mov->MaxSwimSpeed = RunSpeed * SwimSpeedFactor;

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

	// Create Enhanced Input actions programmatically
	MoveAction = CreateInputAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = CreateInputAction(this, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	JumpAction = CreateInputAction(this, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	ZoomAction = CreateInputAction(this, TEXT("IA_Zoom"), EInputActionValueType::Axis1D);
	ToggleWalkAction = CreateInputAction(this, TEXT("IA_ToggleWalk"), EInputActionValueType::Boolean);
	AutoRunAction = CreateInputAction(this, TEXT("IA_AutoRun"), EInputActionValueType::Boolean);

	// Create mapping context and bind keys
	GameplayContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Gameplay"));

	// WASD movement
	{
		FEnhancedActionKeyMapping& W = GameplayContext->MapKey(MoveAction, EKeys::W);
		auto* SwizzleW = NewObject<UInputModifierSwizzleAxis>(GameplayContext);
		SwizzleW->Order = EInputAxisSwizzle::YXZ;
		W.Modifiers.Add(SwizzleW);
	}
	{
		FEnhancedActionKeyMapping& S = GameplayContext->MapKey(MoveAction, EKeys::S);
		auto* SwizzleS = NewObject<UInputModifierSwizzleAxis>(GameplayContext);
		SwizzleS->Order = EInputAxisSwizzle::YXZ;
		S.Modifiers.Add(SwizzleS);
		auto* NegateS = NewObject<UInputModifierNegate>(GameplayContext);
		S.Modifiers.Add(NegateS);
	}
	{
		GameplayContext->MapKey(MoveAction, EKeys::D);
	}
	{
		FEnhancedActionKeyMapping& A = GameplayContext->MapKey(MoveAction, EKeys::A);
		auto* NegateA = NewObject<UInputModifierNegate>(GameplayContext);
		A.Modifiers.Add(NegateA);
	}

	// Mouse look
	GameplayContext->MapKey(LookAction, EKeys::Mouse2D);

	// Jump
	GameplayContext->MapKey(JumpAction, EKeys::SpaceBar);

	// Zoom (mouse wheel)
	GameplayContext->MapKey(ZoomAction, EKeys::MouseWheelAxis);

	// Walk/run toggle (/ key, like WoW)
	GameplayContext->MapKey(ToggleWalkAction, EKeys::Slash);

	// Auto-run (Num Lock)
	GameplayContext->MapKey(AutoRunAction, EKeys::NumLock);
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

	// First-person mode at minimum zoom
	if (FollowCamera && CameraBoom)
	{
		const bool bFirstPerson = CameraBoom->TargetArmLength <= MinCameraDistance + 10.0f;
		// Hide character mesh in first person (when mesh exists)
		if (GetMesh())
		{
			GetMesh()->SetOwnerNoSee(bFirstPerson);
		}
	}

	// Auto-run: inject forward movement each tick
	if (bIsAutoRunning)
	{
		const FRotator CameraRot(0.0f, CameraYaw, 0.0f);
		const FVector Forward = FRotationMatrix(CameraRot).GetUnitAxis(EAxis::X);
		AddMovementInput(Forward, 1.0f);
	}

	// Fall tracking
	UCharacterMovementComponent* Mov = GetCharacterMovement();
	if (Mov && Mov->IsFalling())
	{
		if (!bIsFalling)
		{
			bIsFalling = true;
			FallStartZ = GetActorLocation().Z;
		}
	}

	// Left mouse drag: turn character to face camera direction
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->IsInputKeyDown(EKeys::LeftMouseButton))
	{
		bLeftMouseTurning = true;
		// Orient character to camera yaw while left mouse is held
		SetActorRotation(FRotator(0.0f, CameraYaw, 0.0f));
	}
	else
	{
		bLeftMouseTurning = false;
	}
}

void AWowPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (bIsFalling)
	{
		bIsFalling = false;
		const float FallDistance = FallStartZ - GetActorLocation().Z;

		if (FallDistance > FallDamageMinHeight)
		{
			// Fall damage: proportional to distance beyond threshold
			const float ExcessFall = FallDistance - FallDamageMinHeight;
			const float DamagePct = FMath::Clamp(ExcessFall / 3000.0f, 0.0f, 1.0f); // max at ~30 extra yards
			UE_LOG(LogWowPlayerChar, Log, TEXT("Fall damage: fell %.0f cm (%.0f excess), %.0f%% damage"),
				FallDistance, ExcessFall, DamagePct * 100.0f);
			// Actual HP damage would be applied via server — log it for now
		}
	}
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
		if (ToggleWalkAction) EI->BindAction(ToggleWalkAction, ETriggerEvent::Started, this, &AWowPlayerCharacter::OnToggleWalk);
		if (AutoRunAction) EI->BindAction(AutoRunAction, ETriggerEvent::Started, this, &AWowPlayerCharacter::OnToggleAutoRun);
	}
}

void AWowPlayerCharacter::OnMove(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	if (Input.IsNearlyZero()) return;

	// Any manual movement cancels auto-run
	if (bIsAutoRunning)
	{
		bIsAutoRunning = false;
		UE_LOG(LogWowPlayerChar, Log, TEXT("Auto-run cancelled by movement input"));
	}

	// Backpedal speed: 60% when moving backward (negative Y = S key)
	UCharacterMovementComponent* Mov = GetCharacterMovement();
	if (Mov)
	{
		const float BaseSpeed = bIsWalking ? RunSpeed * WalkSpeedFactor : RunSpeed;
		if (Input.Y < -0.1f)
		{
			// Backpedaling
			Mov->MaxWalkSpeed = BaseSpeed * BackpedalFactor;
		}
		else
		{
			Mov->MaxWalkSpeed = BaseSpeed;
		}
	}

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

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// Right mouse drag: orbit camera without turning character
	// Left mouse drag: turn character (handled in Tick via bLeftMouseTurning)
	// Both behaviors orbit the camera
	if (PC->IsInputKeyDown(EKeys::RightMouseButton) || PC->IsInputKeyDown(EKeys::LeftMouseButton))
	{
		CameraYaw += Input.X * 0.15f;
		CameraPitch = FMath::Clamp(CameraPitch + Input.Y * -0.15f, -80.0f, 10.0f);
	}
}

void AWowPlayerCharacter::OnJumpPressed()
{
	// Swimming: space = swim up (handled by character movement in swimming mode)
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

void AWowPlayerCharacter::OnToggleWalk()
{
	bIsWalking = !bIsWalking;

	UCharacterMovementComponent* Mov = GetCharacterMovement();
	if (Mov)
	{
		Mov->MaxWalkSpeed = bIsWalking ? RunSpeed * WalkSpeedFactor : RunSpeed;
	}

	UE_LOG(LogWowPlayerChar, Log, TEXT("Movement mode: %s (speed: %.0f cm/s)"),
		bIsWalking ? TEXT("Walk") : TEXT("Run"),
		Mov ? Mov->MaxWalkSpeed : 0.0f);
}

void AWowPlayerCharacter::OnToggleAutoRun()
{
	bIsAutoRunning = !bIsAutoRunning;
	UE_LOG(LogWowPlayerChar, Log, TEXT("Auto-run: %s"), bIsAutoRunning ? TEXT("ON") : TEXT("OFF"));
}

void AWowPlayerCharacter::ApplyServerSpeeds(float ServerRunSpeed, float ServerWalkSpeed)
{
	RunSpeed = ServerRunSpeed * 100.0f; // WoW units → cm
	WalkSpeedFactor = (ServerRunSpeed > 0.0f) ? (ServerWalkSpeed / ServerRunSpeed) : 0.357f;

	if (UCharacterMovementComponent* Mov = GetCharacterMovement())
	{
		Mov->MaxWalkSpeed = bIsWalking ? RunSpeed * WalkSpeedFactor : RunSpeed;
		Mov->MaxSwimSpeed = RunSpeed * SwimSpeedFactor;
	}
}

void AWowPlayerCharacter::ApplyLoginSpawn(const FVector& SpawnPos, float OrientationRadians)
{
	// Aerial mode: used for automated verification screenshots (stable top-down view)
	const bool bIsAutomatedCapture = FParse::Param(FCommandLine::Get(), TEXT("aerialview"));
	const float CapsuleHalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
	FVector StabilizedPos = SpawnPos + FVector(0.0f, 0.0f, CapsuleHalfHeight + 50.0f);
	const float YawDegrees = FMath::RadiansToDegrees(OrientationRadians);

	if (bIsAutomatedCapture)
	{
		// Verification screenshots need a stable aerial view
		StabilizedPos.Z += 20000.0f;
	}

	SetActorLocation(StabilizedPos, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(FRotator(0.0f, YawDegrees, 0.0f));

	CameraYaw = YawDegrees;
	CameraPitch = bIsAutomatedCapture ? -80.0f : -30.0f;
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = bIsAutomatedCapture
			? FMath::Max(CameraBoom->TargetArmLength, 4500.0f)
			: FMath::Max(CameraBoom->TargetArmLength, 900.0f);
		CameraBoom->SetWorldRotation(FRotator(CameraPitch, CameraYaw, 0.0f));
	}

	if (UCharacterMovementComponent* Mov = GetCharacterMovement())
	{
		if (bIsAutomatedCapture)
		{
			// Automated captures: keep flying mode for stable aerial screenshots
			Mov->GravityScale = 0.0f;
			Mov->Velocity = FVector::ZeroVector;
			Mov->SetMovementMode(MOVE_Flying);
		}
		else
		{
			// Gameplay mode: enable gravity and terrain following
			Mov->GravityScale = 1.5f;
			Mov->Velocity = FVector::ZeroVector;
			Mov->SetMovementMode(MOVE_Walking);
			UE_LOG(LogWowPlayerChar, Log, TEXT("Login spawn: gravity enabled, walking mode active"));
		}
	}
}
