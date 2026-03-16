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
#include "WowCharacterBuilder.h"
#include "WowCharacterTexture.h"
#include "WowAssetCache.h"
#include "Mpq/MpqManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"

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

	// Standard UE5 third person setup
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Movement — character faces movement direction
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

	// Spring arm — follows controller rotation (standard UE5 3rd person)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->ProbeSize = 12.0f;
	CameraBoom->ProbeChannel = ECC_Camera;

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

}

void AWowPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Auto-run: inject forward movement each tick
	if (bIsAutoRunning)
	{
		const FRotator ControlRot = GetControlRotation();
		const FVector Forward = FRotationMatrix(FRotator(0, ControlRot.Yaw, 0)).GetUnitAxis(EAxis::X);
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

	if (bIsAutoRunning)
	{
		bIsAutoRunning = false;
	}

	// Movement relative to controller (camera) facing — standard UE5 3rd person
	const FRotator ControlRot = GetControlRotation();
	const FRotator YawRot(0.0f, ControlRot.Yaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Input.Y); // W/S
	AddMovementInput(Right, Input.X);   // A/D
}

void AWowPlayerCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();

	// Standard UE5: right mouse held = orbit camera
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	if (PC->IsInputKeyDown(EKeys::RightMouseButton))
	{
		AddControllerYawInput(Input.X);
		AddControllerPitchInput(Input.Y);
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

	// Set controller rotation so spring arm follows
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		float Pitch = bIsAutomatedCapture ? -80.0f : -20.0f;
		PC->SetControlRotation(FRotator(Pitch, YawDegrees, 0.0f));
	}
	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = bIsAutomatedCapture ? 4500.0f : 400.0f;
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

void AWowPlayerCharacter::SetCharacterModel(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
	uint8 Race, uint8 Gender, uint8 SkinColor, uint8 Face, uint8 HairStyle, uint8 HairColor, uint8 FacialHair)
{
	if (!World || !Mpq || !Cache)
	{
		UE_LOG(LogWowPlayerChar, Warning, TEXT("SetCharacterModel: Missing required parameters"));
		return;
	}

	UE_LOG(LogWowPlayerChar, Log, TEXT("Setting character model: Race=%d Gender=%d Skin=%d Face=%d Hair=%d/%d FacialHair=%d"),
		Race, Gender, SkinColor, Face, HairStyle, HairColor, FacialHair);

	// Remove any existing skeletal mesh components (except the default one)
	TArray<USkeletalMeshComponent*> ExistingMeshes;
	GetComponents<USkeletalMeshComponent>(ExistingMeshes);
	for (USkeletalMeshComponent* Mesh : ExistingMeshes)
	{
		if (Mesh != GetMesh()) // Keep the default character mesh component
		{
			Mesh->DestroyComponent();
		}
	}

	// Build character parameters
	FWowCharacterBuilder::FCharacterParams Params;
	Params.Race = static_cast<FWowCharacterBuilder::ERace>(Race);
	Params.Gender = static_cast<FWowCharacterBuilder::EGender>(Gender);
	Params.Customization.RaceId = Race;
	Params.Customization.Gender = Gender;
	Params.Customization.SkinColor = SkinColor;
	Params.Customization.FaceVariation = Face;
	Params.Customization.HairStyle = HairStyle;
	Params.Customization.HairColor = HairColor;
	Params.Customization.FacialHairStyle = FacialHair;

	// Get character model path
	FString ModelPath = FWowCharacterBuilder::GetCharacterModelPath(Params.Race, Params.Gender);
	if (ModelPath.IsEmpty())
	{
		UE_LOG(LogWowPlayerChar, Warning, TEXT("No model path found for Race=%d Gender=%d"), Race, Gender);
		return;
	}

	// Build composite texture
	UTexture2D* CompositeTex = FWowCharacterTexture::BuildCompositeTexture(Mpq, Cache, Params.Customization);

	// Build skeletal mesh and components using the same M2 actor logic from CharacterBuilder
	// but attach to our existing character instead of creating a new actor

	// TODO: This is a simplified approach - we need to integrate the full character building logic
	// For now, let's at least set the mesh on the existing character mesh component

	// Create a temporary actor to build the character, then extract its components
	FVector TempLocation = GetActorLocation() + FVector(0, 0, -10000); // Spawn far below to avoid visual artifacts
	FRotator TempRotation = GetActorRotation();

	AActor* TempCharacterActor = nullptr;

	// Try the full equipment version first
	if (Params.Equipment.Num() > 0 ||
		(Params.BodyEquipment.ChestDisplayId > 0 || Params.BodyEquipment.PantsDisplayId > 0 ||
		 Params.BodyEquipment.BootsDisplayId > 0 || Params.BodyEquipment.GlovesDisplayId > 0))
	{
		TempCharacterActor = FWowCharacterBuilder::SpawnCharacterWithEquipment(
			World, Mpq, Cache, Params, TempLocation, TempRotation);
	}

	// Fallback to simple character spawn without equipment if full version fails
	if (!TempCharacterActor)
	{
		UE_LOG(LogWowPlayerChar, Log, TEXT("Falling back to simple character spawn without equipment"));
		TempCharacterActor = FWowCharacterBuilder::SpawnCharacter(
			World, Mpq, Cache, Params.Race, Params.Gender, TempLocation, TempRotation);
	}

	if (!TempCharacterActor)
	{
		UE_LOG(LogWowPlayerChar, Warning, TEXT("Failed to spawn any character for model extraction"));
		return;
	}

	// Extract skeletal mesh components from the temporary actor
	TArray<USkeletalMeshComponent*> TempMeshes;
	TempCharacterActor->GetComponents<USkeletalMeshComponent>(TempMeshes);

	if (TempMeshes.Num() > 0)
	{
		// Copy the main skeletal mesh to our character's mesh component
		USkeletalMeshComponent* MainMesh = TempMeshes[0];
		if (MainMesh && MainMesh->GetSkeletalMeshAsset())
		{
			GetMesh()->SetSkeletalMesh(MainMesh->GetSkeletalMeshAsset());
			GetMesh()->SetAnimationMode(MainMesh->GetAnimationMode());

			// Copy materials
			for (int32 MatIdx = 0; MatIdx < MainMesh->GetNumMaterials(); ++MatIdx)
			{
				GetMesh()->SetMaterial(MatIdx, MainMesh->GetMaterial(MatIdx));
			}

			// Copy animation: use SingleNode mode and play the same animation
			GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			if (UAnimSequence* PlayingAnim = Cast<UAnimSequence>(MainMesh->AnimationData.AnimToPlay.Get()))
			{
				GetMesh()->PlayAnimation(PlayingAnim, true);
				UE_LOG(LogWowPlayerChar, Log, TEXT("Playing animation from temp actor"));
			}
			else
			{
				UE_LOG(LogWowPlayerChar, Log, TEXT("No animation playing on temp actor mesh"));
			}

			// Position mesh so feet are at capsule bottom
			GetMesh()->SetRelativeLocation(FVector(0, 0, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));

			UE_LOG(LogWowPlayerChar, Log, TEXT("Applied main character mesh to player"));
		}

		// Handle additional meshes (like separate hair) - attach as child components
		for (int32 i = 1; i < TempMeshes.Num(); ++i)
		{
			USkeletalMeshComponent* ExtraMesh = TempMeshes[i];
			if (ExtraMesh && ExtraMesh->GetSkeletalMeshAsset())
			{
				// Create a new skeletal mesh component on our character
				FString ComponentName = FString::Printf(TEXT("CharacterMesh_%d"), i);
				USkeletalMeshComponent* NewMeshComp = NewObject<USkeletalMeshComponent>(this, *ComponentName);
				NewMeshComp->SetupAttachment(GetMesh());
				NewMeshComp->SetSkeletalMesh(ExtraMesh->GetSkeletalMeshAsset());
				NewMeshComp->SetCastShadow(true);
				NewMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				NewMeshComp->SetLeaderPoseComponent(GetMesh()); // Follow main mesh animations

				// Copy materials
				for (int32 MatIdx = 0; MatIdx < ExtraMesh->GetNumMaterials(); ++MatIdx)
				{
					NewMeshComp->SetMaterial(MatIdx, ExtraMesh->GetMaterial(MatIdx));
				}

				NewMeshComp->RegisterComponent();
				UE_LOG(LogWowPlayerChar, Log, TEXT("Added additional character mesh component: %s"), *ComponentName);
			}
		}
	}

	// Clean up the temporary actor
	TempCharacterActor->Destroy();

	UE_LOG(LogWowPlayerChar, Log, TEXT("Character model set successfully"));
}

void AWowPlayerCharacter::SetupAnimationController(UWowAnimationController* AnimController)
{
	this->AnimationController = AnimController;
	UE_LOG(LogWowPlayerChar, Log, TEXT("Animation controller setup for local player character"));
}
