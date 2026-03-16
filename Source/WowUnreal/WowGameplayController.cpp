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
#include "WowNameplateWidget.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Coord/WowCoordinate.h"
#include "Components/WidgetComponent.h"

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
	// Override spawn position: Northshire Abbey at safe altitude for testing
	if (FString(FCommandLine::Get()).Contains(TEXT("startpos")))
	{
		X = -8949.0f; Y = -132.0f; Z = 99.0f; // 15m above terrain (84 + 15)
		UE_LOG(LogWowGameplay, Warning, TEXT("Overriding spawn to Northshire Abbey (aerial): (%.1f, %.1f, %.1f)"), X, Y, Z);
	}

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
			// When -startpos is active, look down at terrain; otherwise use server orientation
			float Pitch = FString(FCommandLine::Get()).Contains(TEXT("startpos")) ? -15.0f : 0.0f;
			P->SetActorRotation(FRotator(Pitch, FMath::RadiansToDegrees(Orientation), 0.0f));
			if (APlayerController* PC = Cast<APlayerController>(P->GetController()))
			{
				PC->SetControlRotation(FRotator(Pitch, FMath::RadiansToDegrees(Orientation), 0.0f));
			}
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

		// Server position correction: if server position diverges too much, teleport back
		// Skip correction when using -startpos (fly camera free-roam mode)
		if (bHasServerPosition && Entity.Movement.Position != FVector::ZeroVector
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

		// Update nameplate health if needed
		TObjectPtr<UWidgetComponent>* NameplatePtr = EntityNameplates.Find(Entity.Guid);
		if (NameplatePtr && *NameplatePtr)
		{
			UWidgetComponent* NameplateComponent = *NameplatePtr;
			if (UWowNameplateWidget* NameplateWidget = Cast<UWowNameplateWidget>(NameplateComponent->GetUserWidgetObject()))
			{
				int32 Health = Entity.GetHealth();
				int32 MaxHealth = Entity.GetMaxHealth();
				if (Health > 0 && MaxHealth > 0)
				{
					NameplateWidget->UpdateHealth(Health, MaxHealth);
				}
			}
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

	// Update entity spline movement
	UpdateEntitySplineMovement(DeltaTime);

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

	// Skip local player — they already have a pawn
	if (Entity.Guid == ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid) return;

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

	// Clean up nameplate
	EntityNameplates.Remove(Guid);
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

		// Create nameplate widget component
		UWidgetComponent* NameplateComponent = NewObject<UWidgetComponent>(SpawnedActor);
		if (NameplateComponent)
		{
			NameplateComponent->SetupAttachment(SpawnedActor->GetRootComponent());
			NameplateComponent->SetWidgetSpace(EWidgetSpace::Screen);
			NameplateComponent->SetDrawSize(FVector2D(200.0f, 40.0f));

			// Position the nameplate above the entity (offset Z by ~200cm)
			NameplateComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 200.0f));

			// Create the nameplate widget class
			UClass* NameplateClass = UWowNameplateWidget::StaticClass();
			if (NameplateClass)
			{
				NameplateComponent->SetWidgetClass(NameplateClass);

				// Get entity name and health
				FString EntityName = TEXT("NPC");
				bool bIsHostile = false;

				if (Entity.IsPlayer())
				{
					EntityName = FString::Printf(TEXT("Player %llu"), Entity.Guid);
					bIsHostile = false; // Assume other players are friendly for now
				}
				else
				{
					EntityName = FString::Printf(TEXT("NPC %d"), Entity.Entry);

					// Determine hostility based on faction (simplified)
					const FWowUnitEntity* UnitEntity = static_cast<const FWowUnitEntity*>(&Entity);
					uint32 FactionTemplate = UnitEntity->GetFactionTemplate();
					// Simple heuristic: most hostile creatures have faction > 100
					bIsHostile = (FactionTemplate > 100 && FactionTemplate != 2000); // 2000 is often neutral/friendly
				}

				int32 Health = Entity.GetHealth();
				int32 MaxHealth = Entity.GetMaxHealth();

				// Initialize the nameplate widget
				if (UWowNameplateWidget* NameplateWidget = Cast<UWowNameplateWidget>(NameplateComponent->GetUserWidgetObject()))
				{
					NameplateWidget->SetupNameplate(EntityName, Health, MaxHealth, bIsHostile);
				}
				else
				{
					// Widget not yet created, store data to initialize later
					UE_LOG(LogWowGameplay, Log, TEXT("Nameplate widget not ready yet for entity %llu, will initialize on first update"), Entity.Guid);
				}

				NameplateComponent->RegisterComponent();
				EntityNameplates.Add(Entity.Guid, NameplateComponent);

				UE_LOG(LogWowGameplay, Log, TEXT("Created nameplate for entity %llu (%s)"), Entity.Guid, *EntityName);
			}
		}
	}
}

void AWowGameplayController::UpdateEntitySplineMovement(float DeltaTime)
{
	if (!ConnectionManager) return;

	// Iterate through all entities and update those with active splines
	FWowEntityManager& EntityManager = ConnectionManager->PacketHandler.EntityManager;

	for (const auto& EntityPair : EntityManager.GetAll())
	{
		FWowEntity* EntityPtr = EntityPair.Value.Get();
		if (!EntityPtr) continue;
		FWowEntity& Entity = *EntityPtr;

		// Skip local player
		if (Entity.Guid == EntityManager.LocalPlayerGuid) continue;

		// Skip entities without active splines
		if (!Entity.Movement.bHasActiveSpline || Entity.Movement.SplineWaypoints.Num() == 0)
			continue;

		// Update spline elapsed time
		Entity.Movement.SplineElapsed += DeltaTime * 1000.0f; // Convert to milliseconds

		// Check if spline is complete
		if (Entity.Movement.SplineElapsed >= Entity.Movement.SplineDuration)
		{
			// Move to final position
			if (Entity.Movement.SplineWaypoints.Num() > 0)
			{
				Entity.Movement.Position = Entity.Movement.SplineWaypoints.Last().Position;
			}

			// Calculate final orientation to face the last waypoint direction
			if (Entity.Movement.SplineWaypoints.Num() > 1)
			{
				const FVector LastWP = Entity.Movement.SplineWaypoints.Last().Position;
				const FVector SecondLastWP = Entity.Movement.SplineWaypoints[Entity.Movement.SplineWaypoints.Num() - 2].Position;
				FVector Dir = FVector(LastWP.X - SecondLastWP.X, LastWP.Y - SecondLastWP.Y, 0.0f);
				if (!Dir.IsNearlyZero())
				{
					Entity.Movement.Orientation = FMath::Atan2(Dir.Y, Dir.X);
				}
			}

			// Clear spline
			Entity.Movement.bHasActiveSpline = false;
			Entity.Movement.SplineWaypoints.Empty();

			UE_LOG(LogWowGameplay, Log, TEXT("Spline movement completed for entity %llu"), Entity.Guid);
		}
		else
		{
			// Interpolate along the spline
			float Progress = Entity.Movement.SplineElapsed / float(Entity.Movement.SplineDuration);
			Progress = FMath::Clamp(Progress, 0.0f, 1.0f);

			// Simple linear interpolation between waypoints
			if (Entity.Movement.SplineWaypoints.Num() == 1)
			{
				// Single waypoint: interpolate from current position to waypoint
				Entity.Movement.Position = FMath::Lerp(
					Entity.Movement.SplineStartPosition,
					Entity.Movement.SplineWaypoints[0].Position,
					Progress);
			}
			else if (Entity.Movement.SplineWaypoints.Num() > 1)
			{
				// Multiple waypoints: treat as segments
				float TotalSegments = float(Entity.Movement.SplineWaypoints.Num());
				float SegmentProgress = Progress * TotalSegments;
				int32 CurrentSegment = FMath::FloorToInt32(SegmentProgress);
				float LocalProgress = SegmentProgress - CurrentSegment;

				if (CurrentSegment == 0)
				{
					// First segment: from start position to first waypoint
					Entity.Movement.Position = FMath::Lerp(
						Entity.Movement.SplineStartPosition,
						Entity.Movement.SplineWaypoints[0].Position,
						LocalProgress);
				}
				else if (CurrentSegment < Entity.Movement.SplineWaypoints.Num())
				{
					// Intermediate segments: from previous waypoint to current waypoint
					Entity.Movement.Position = FMath::Lerp(
						Entity.Movement.SplineWaypoints[CurrentSegment - 1].Position,
						Entity.Movement.SplineWaypoints[CurrentSegment].Position,
						LocalProgress);
				}
				else
				{
					// Final waypoint
					Entity.Movement.Position = Entity.Movement.SplineWaypoints.Last().Position;
				}
			}

			// Update orientation to face movement direction
			if (Entity.Movement.SplineWaypoints.Num() > 0)
			{
				FVector TargetPos;
				if (Entity.Movement.SplineWaypoints.Num() == 1)
				{
					TargetPos = Entity.Movement.SplineWaypoints[0].Position;
				}
				else
				{
					// Face the next waypoint
					float TotalSegments = float(Entity.Movement.SplineWaypoints.Num());
					float SegmentProgress = Progress * TotalSegments;
					int32 NextSegment = FMath::CeilToInt32(SegmentProgress);
					NextSegment = FMath::Clamp(NextSegment, 0, Entity.Movement.SplineWaypoints.Num() - 1);
					TargetPos = Entity.Movement.SplineWaypoints[NextSegment].Position;
				}

				FVector Dir = FVector(TargetPos.X - Entity.Movement.Position.X, TargetPos.Y - Entity.Movement.Position.Y, 0.0f);
				if (!Dir.IsNearlyZero())
				{
					Entity.Movement.Orientation = FMath::Atan2(Dir.Y, Dir.X);
				}
			}
		}

		// Update the visual actor position
		TObjectPtr<AActor>* ActorPtr = SpawnedEntityActors.Find(Entity.Guid);
		if (ActorPtr && *ActorPtr)
		{
			FVector UEPos = FWowCoordinate::WowToUE(Entity.Movement.Position);
			(*ActorPtr)->SetActorLocation(UEPos);
			(*ActorPtr)->SetActorRotation(FRotator(0.0f, FMath::RadiansToDegrees(Entity.Movement.Orientation), 0.0f));
		}
	}
}
