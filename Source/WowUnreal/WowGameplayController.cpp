#include "WowGameplayController.h"
#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "WowEntity.h"
#include "WowOpcodes.h"
#include "WowPlayerCharacter.h"
#include "WowUIManager.h"
#include "WowEventSystem.h"
#include "GameFramework/Character.h"
#include "Coord/WowCoordinate.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowGameplay, Log, All);

AWowGameplayController::AWowGameplayController()
{
	bShowMouseCursor = false;
	PrimaryActorTick.bCanEverTick = true;
}

void AWowGameplayController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameOnly());

	// Cache UIManager early so OnUpdate ticks even without networking
	if (UGameInstance* GI = GetGameInstance())
	{
		UIManager = GI->GetSubsystem<UWowUIManager>();
	}
}

void AWowGameplayController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Left click for targeting
	InputComponent->BindAction(TEXT("LeftClick"), IE_Released, this, &AWowGameplayController::OnLeftClick);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AWowGameplayController::OnLeftClick);
}

void AWowGameplayController::BindEntityEvents()
{
	if (!ConnectionManager) return;

	// Listen for login verify world — teleport pawn to spawn position
	ConnectionManager->PacketHandler.OnLoginVerifyWorld.AddUObject(
		this, &AWowGameplayController::OnLoginVerifyWorld);

	// Listen for entity updates — sync local player position from server
	ConnectionManager->PacketHandler.EntityManager.OnEntityUpdated.AddUObject(
		this, &AWowGameplayController::OnEntityUpdated);

	// Forward SMSG opcodes to UI event system
	ConnectionManager->PacketHandler.OnOpcodeReceived.AddUObject(
		this, &AWowGameplayController::OnOpcodeReceived);

	// Cache UIManager for tick dispatch
	if (UGameInstance* GI = GetGameInstance())
	{
		UIManager = GI->GetSubsystem<UWowUIManager>();
	}
}

void AWowGameplayController::OnLoginVerifyWorld(uint32 MapId, float X, float Y, float Z, float Orientation)
{
	FVector SpawnPos = FWowCoordinate::WowToUE(X, Y, Z);

	APawn* P = GetPawn();
	if (P)
	{
		if (AWowPlayerCharacter* PlayerCharacter = Cast<AWowPlayerCharacter>(P))
		{
			PlayerCharacter->ApplyLoginSpawn(SpawnPos, Orientation);
		}
		else
		{
			P->SetActorLocation(SpawnPos);
			P->SetActorRotation(FRotator(0.0f, FMath::RadiansToDegrees(Orientation), 0.0f));
		}

		UE_LOG(LogWowGameplay, Log, TEXT("Teleported to spawn: map=%d wow=(%.1f,%.1f,%.1f) orient=%.2f ue=(%.0f,%.0f,%.0f)"),
			MapId, X, Y, Z, Orientation, SpawnPos.X, SpawnPos.Y, SpawnPos.Z);
	}

	bHasServerPosition = true;
	LastServerPosition = SpawnPos;
}

void AWowGameplayController::OnEntityUpdated(const FWowEntity& Entity)
{
	if (!ConnectionManager) return;

	// Only care about local player entity
	if (Entity.Guid != ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid) return;

	// Apply server speeds to character
	if (Entity.Movement.RunSpeed > 0.0f)
	{
		if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn()))
		{
			PlayerChar->ApplyServerSpeeds(Entity.Movement.RunSpeed, Entity.Movement.WalkSpeed);
		}
	}

	// Server position correction: if server position diverges too much, teleport back
	if (bHasServerPosition && Entity.Movement.Position != FVector::ZeroVector)
	{
		FVector ServerUEPos = FWowCoordinate::WowToUE(
			Entity.Movement.Position.X,
			Entity.Movement.Position.Y,
			Entity.Movement.Position.Z);

		APawn* P = GetPawn();
		if (P)
		{
			const float Dist = FVector::Dist(P->GetActorLocation(), ServerUEPos);
			if (Dist > ServerCorrectionThreshold)
			{
				UE_LOG(LogWowGameplay, Warning, TEXT("Server correction: client diverged %.0f cm from server, teleporting"),
					Dist);
				P->SetActorLocation(ServerUEPos, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}

		LastServerPosition = ServerUEPos;
	}
}

void AWowGameplayController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Dispatch OnUpdate to all WoW UI frames each tick
	if (UIManager && UIManager->GetEventSystem())
	{
		UIManager->GetEventSystem()->TickOnUpdate(DeltaTime);
	}

	if (!ConnectionManager) return;

	// Movement heartbeat — send position to server while moving
	MovementSyncTimer += DeltaTime;
	if (MovementSyncTimer >= MovementSyncInterval)
	{
		MovementSyncTimer = 0.0f;
		SendMovementUpdate();
	}

	// Keep-alive
	KeepAliveTimer += DeltaTime;
	if (KeepAliveTimer >= KeepAliveInterval)
	{
		KeepAliveTimer = 0.0f;
		ConnectionManager->SendKeepAlive();
	}
}

void AWowGameplayController::SendMovementUpdate()
{
	APawn* P = GetPawn();
	if (!P || !bHasServerPosition) return;

	FVector Pos = P->GetActorLocation();

	// Only send if position actually changed
	if (Pos.Equals(LastSentPosition, 1.0f)) return;
	LastSentPosition = Pos;

	FVector WowPos = FWowCoordinate::UEToWow(Pos);

	float Orientation = FMath::DegreesToRadians(P->GetActorRotation().Yaw);

	ConnectionManager->SendMovement(WowOpcode::MSG_MOVE_HEARTBEAT, WowPos, Orientation, 0);
}

void AWowGameplayController::OnOpcodeReceived(uint16 Opcode)
{
	if (!UIManager) return;

	FWowEventSystem* EventSystem = UIManager->GetEventSystem();
	if (!EventSystem) return;

	FString EventName = FWowEventSystem::OpcodeToEvent(Opcode);
	if (!EventName.IsEmpty())
	{
		EventSystem->FireEvent(EventName);
	}
}

void AWowGameplayController::OnLeftClick()
{
	TryTargetUnderCursor();
}

void AWowGameplayController::TryTargetUnderCursor()
{
	if (!ConnectionManager) return;

	// Line trace from mouse cursor into the world
	FHitResult Hit;
	if (GetHitResultUnderCursor(ECC_Pawn, false, Hit))
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor)
		{
			// Check if the hit actor has an associated entity GUID (stored as tag)
			FString GuidTag = HitActor->Tags.Num() > 0 ? HitActor->Tags[0].ToString() : TEXT("");
			if (!GuidTag.IsEmpty())
			{
				uint64 HitGuid = FCString::Atoi64(*GuidTag);
				if (HitGuid != 0 && HitGuid != TargetGuid)
				{
					TargetGuid = HitGuid;
					ConnectionManager->SendSetSelection(static_cast<int64>(TargetGuid));
					UE_LOG(LogWowGameplay, Log, TEXT("Targeted entity GUID: %llu"), TargetGuid);
				}
			}
		}
	}
	else
	{
		// Clicked on nothing — clear target
		if (TargetGuid != 0)
		{
			TargetGuid = 0;
			ConnectionManager->SendSetSelection(0);
			UE_LOG(LogWowGameplay, Log, TEXT("Target cleared"));
		}
	}
}
