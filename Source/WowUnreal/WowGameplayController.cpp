#include "WowGameplayController.h"
#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "WowEntity.h"
#include "WowOpcodes.h"
#include "WowPlayerCharacter.h"
#include "WowUIManager.h"
#include "WowEventSystem.h"
#include "WowWorldManager.h"
#include "WowCharacterBuilder.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Coord/WowCoordinate.h"
#include "Engine/World.h"
#include "TimerManager.h"

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

	// Listen for entity creation/destruction — spawn/destroy character models
	ConnectionManager->PacketHandler.EntityManager.OnEntityCreated.AddUObject(
		this, &AWowGameplayController::OnEntityCreated);
	ConnectionManager->PacketHandler.EntityManager.OnEntityDestroyed.AddUObject(
		this, &AWowGameplayController::OnEntityDestroyed);

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

	UE_LOG(LogWowGameplay, Log, TEXT("LoginVerifyWorld: map=%d wow=(%.1f,%.1f,%.1f) orient=%.2f ue=(%.0f,%.0f,%.0f)"),
		MapId, X, Y, Z, Orientation, SpawnPos.X, SpawnPos.Y, SpawnPos.Z);

	if (bDeferSpawnTeleport)
	{
		// Store for later — LoginController will call ApplyDeferredSpawn() after terrain loads
		DeferredSpawnPos = SpawnPos;
		DeferredSpawnOrientation = Orientation;
		bHasDeferredSpawn = true;
		UE_LOG(LogWowGameplay, Log, TEXT("Spawn deferred — waiting for terrain to load"));
		return;
	}

	// Immediate teleport (legacy path)
	ApplyDeferredSpawn_Internal(SpawnPos, Orientation);
}

void AWowGameplayController::ApplyDeferredSpawn()
{
	if (!bHasDeferredSpawn) return;
	bHasDeferredSpawn = false;
	ApplyDeferredSpawn_Internal(DeferredSpawnPos, DeferredSpawnOrientation);
}

void AWowGameplayController::ApplyDeferredSpawn_Internal(const FVector& SpawnPos, float Orientation)
{
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
			float Pitch = 0.0f;
			P->SetActorRotation(FRotator(Pitch, FMath::RadiansToDegrees(Orientation), 0.0f));
			if (APlayerController* PC = Cast<APlayerController>(P->GetController()))
			{
				PC->SetControlRotation(FRotator(Pitch, FMath::RadiansToDegrees(Orientation), 0.0f));
			}
		}

		UE_LOG(LogWowGameplay, Log, TEXT("Teleported to spawn: ue=(%.0f,%.0f,%.0f)"),
			SpawnPos.X, SpawnPos.Y, SpawnPos.Z);
	}

	bHasServerPosition = true;
	LastServerPosition = SpawnPos;
}

void AWowGameplayController::OnEntityUpdated(const FWowEntity& Entity)
{
	if (!ConnectionManager) return;

	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;

	if (Entity.Guid == LocalGuid)
	{
		// Apply server speeds to character
		if (Entity.Movement.RunSpeed > 0.0f)
		{
			if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn()))
			{
				PlayerChar->ApplyServerSpeeds(Entity.Movement.RunSpeed, Entity.Movement.WalkSpeed);
			}
		}

		// Check if this is the first time we're receiving entity data for the local player
		// and set up character model if we haven't already
		if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn()))
		{
			// Check if the player already has a skeletal mesh set
			if (!PlayerChar->GetMesh()->GetSkeletalMeshAsset())
			{
				SetupLocalPlayerCharacterModel(Entity);
			}
		}

		// Server position correction: if server position diverges too much, teleport back
		// Skip while spawn is deferred (loading screen) or using -startpos
		if (bHasServerPosition && !bDeferSpawnTeleport
			&& Entity.Movement.Position != FVector::ZeroVector
			&& !FString(FCommandLine::Get()).Contains(TEXT("startpos")))
		{
			FVector ServerUEPos = FWowCoordinate::WowToUE(Entity.Movement.Position);

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
	else
	{
		// Non-local entity: update actor position if we spawned one
		TObjectPtr<AActor>* ActorPtr = SpawnedEntityActors.Find(Entity.Guid);
		if (ActorPtr && *ActorPtr && Entity.Movement.Position != FVector::ZeroVector)
		{
			FVector UEPos = FWowCoordinate::WowToUE(Entity.Movement.Position);
			(*ActorPtr)->SetActorLocation(UEPos);
			(*ActorPtr)->SetActorRotation(FRotator(0.0f, FMath::RadiansToDegrees(Entity.Movement.Orientation), 0.0f));
		}
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

void AWowGameplayController::CacheWorldResources()
{
	if (CachedMpq && CachedAssetCache) return;

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWowWorldManager::StaticClass(), Found);
	if (Found.Num() > 0)
	{
		AWowWorldManager* WM = Cast<AWowWorldManager>(Found[0]);
		if (WM)
		{
			CachedMpq = WM->GetMpqManager();
			CachedAssetCache = WM->GetAssetCache();
		}
	}
}

void AWowGameplayController::OnEntityCreated(const FWowEntity& Entity)
{
	if (!ConnectionManager) return;

	// Handle local player differently - apply character model to existing pawn
	if (Entity.Guid == ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid)
	{
		SetupLocalPlayerCharacterModel(Entity);
		return;
	}

	// Only spawn models for units and players
	if (!Entity.IsUnit() && !Entity.IsPlayer()) return;

	// Skip if already spawned
	if (SpawnedEntityActors.Contains(Entity.Guid)) return;

	SpawnEntityModel(Entity);
}

void AWowGameplayController::OnEntityDestroyed(uint64 Guid)
{
	TObjectPtr<AActor>* ActorPtr = SpawnedEntityActors.Find(Guid);
	if (ActorPtr && *ActorPtr)
	{
		(*ActorPtr)->Destroy();
		UE_LOG(LogWowGameplay, Log, TEXT("Destroyed entity model for GUID %llu"), Guid);
	}
	SpawnedEntityActors.Remove(Guid);
}

void AWowGameplayController::SpawnEntityModel(const FWowEntity& Entity)
{
	CacheWorldResources();
	if (!CachedMpq || !CachedAssetCache) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Convert entity position to UE coordinates
	FVector UEPos = FVector::ZeroVector;
	if (Entity.Movement.Position != FVector::ZeroVector)
	{
		UEPos = FWowCoordinate::WowToUE(Entity.Movement.Position);
	}
	FRotator Rot(0.0f, FMath::RadiansToDegrees(Entity.Movement.Orientation), 0.0f);

	AActor* SpawnedActor = nullptr;

	if (Entity.IsPlayer())
	{
		// Players: spawn by race/gender from UNIT_BYTES_0
		const FWowUnitEntity* UnitEntity = static_cast<const FWowUnitEntity*>(&Entity);
		uint8 RaceId = UnitEntity->GetRaceId();
		uint8 GenderId = UnitEntity->GetGenderId();

		if (RaceId > 0)
		{
			auto Race = static_cast<FWowCharacterBuilder::ERace>(RaceId);
			auto Gender = static_cast<FWowCharacterBuilder::EGender>(GenderId);

			SpawnedActor = FWowCharacterBuilder::SpawnCharacter(
				World, CachedMpq, CachedAssetCache, Race, Gender, UEPos, Rot);

			UE_LOG(LogWowGameplay, Log, TEXT("Spawned player model: GUID=%llu Race=%d Gender=%d at %s"),
				Entity.Guid, RaceId, GenderId, *UEPos.ToString());
		}
	}
	else
	{
		// NPCs/creatures: spawn by DisplayId
		uint32 DisplayId = Entity.GetDisplayId();
		if (DisplayId == 0)
		{
			DisplayId = Entity.GetNativeDisplayId();
		}

		if (DisplayId > 0)
		{
			SpawnedActor = FWowCharacterBuilder::SpawnCreatureByDisplayId(
				World, CachedMpq, CachedAssetCache, DisplayId, UEPos, Rot);

			UE_LOG(LogWowGameplay, Log, TEXT("Spawned creature model: GUID=%llu DisplayId=%d at %s"),
				Entity.Guid, DisplayId, *UEPos.ToString());
		}
	}

	if (SpawnedActor)
	{
		// Tag actor with GUID for targeting
		SpawnedActor->Tags.Add(FName(*FString::Printf(TEXT("%llu"), Entity.Guid)));
		SpawnedEntityActors.Add(Entity.Guid, SpawnedActor);
	}
}

void AWowGameplayController::SetupLocalPlayerCharacterModel(const FWowEntity& Entity)
{
	CacheWorldResources();
	if (!CachedMpq || !CachedAssetCache)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Cannot setup local player model - missing world resources. Retrying later..."));

		// Try again in 1 second - the world manager might not be ready yet
		GetWorldTimerManager().SetTimer(
			FTimerHandle(),
			[this, Entity]() { SetupLocalPlayerCharacterModel(Entity); },
			1.0f,
			false
		);
		return;
	}

	AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn());
	if (!PlayerChar)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Cannot setup local player model - no AWowPlayerCharacter pawn"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// Extract race/gender from entity BYTES_0 or cached character info
	uint8 RaceId = 1;  // Default to Human
	uint8 Gender = 0;  // Default to Male
	uint8 SkinColor = 0;
	uint8 Face = 0;
	uint8 HairStyle = 0;
	uint8 HairColor = 0;
	uint8 FacialHair = 0;

	if (Entity.IsUnit())
	{
		const FWowUnitEntity* UnitEntity = static_cast<const FWowUnitEntity*>(&Entity);
		RaceId = UnitEntity->GetRaceId();
		Gender = UnitEntity->GetGenderId();
		UE_LOG(LogWowGameplay, Log, TEXT("Local player race/gender from entity: Race=%d Gender=%d"), RaceId, Gender);
	}

	// If no valid race/gender from entity, try to get from cached character list
	if (RaceId == 0 && ConnectionManager)
	{
		const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;
		const TArray<FWowCharacterInfo>& CachedChars = ConnectionManager->GetCachedCharacters();

		for (const FWowCharacterInfo& CharInfo : CachedChars)
		{
			if (static_cast<uint64>(CharInfo.Guid) == LocalGuid)
			{
				RaceId = CharInfo.Race;
				Gender = CharInfo.Gender;
				UE_LOG(LogWowGameplay, Log, TEXT("Local player race/gender from cached character: Race=%d Gender=%d"), RaceId, Gender);
				break;
			}
		}
	}

	// Fallback to default values if still not found
	if (RaceId == 0)
	{
		RaceId = 1; // Human
		Gender = 0; // Male
		UE_LOG(LogWowGameplay, Warning, TEXT("Using fallback race/gender for local player: Human Male"));
	}

	// TODO: Extract customization data from player entity fields (PLAYER_BYTES, etc.)
	// For now, use default customization values

	// Set the character model on the player pawn
	PlayerChar->SetCharacterModel(World, CachedMpq, CachedAssetCache,
		RaceId, Gender, SkinColor, Face, HairStyle, HairColor, FacialHair);
}
