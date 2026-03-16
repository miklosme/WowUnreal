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
#include "SWowCastBar.h"
#include "WowFloatingText.h"
#include "Formats/Dbc/DbcStore.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/SCanvas.h"

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

	// Create cast bar widget wrapped in positioning container
	TSharedRef<SWidget> CastBarContainer =
		SNew(SCanvas)
		+ SCanvas::Slot()
		.Position(TAttribute<FVector2D>::Create(TAttribute<FVector2D>::FGetter::CreateLambda([this]()
		{
			// Position at bottom center of screen
			FVector2D ViewportSize;
			if (GEngine && GEngine->GameViewport)
			{
				GEngine->GameViewport->GetViewportSize(ViewportSize);
			}
			return FVector2D(ViewportSize.X * 0.5f - 150.0f, ViewportSize.Y - 100.0f); // Center horizontally, near bottom
		})))
		.Size(FVector2D(300.0f, 40.0f))
		[
			SAssignNew(CastBarWidget, SWowCastBar)
		];

	// Add cast bar to viewport
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetForPlayer(GetLocalPlayer(), CastBarContainer, 1000);
	}
}

void AWowGameplayController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Left click for targeting
	InputComponent->BindAction(TEXT("LeftClick"), IE_Released, this, &AWowGameplayController::OnLeftClick);
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AWowGameplayController::OnLeftClick);

	// Right click for auto-attack
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AWowGameplayController::OnRightClick);

	// Spell casting keys (1-6)
	InputComponent->BindKey(EKeys::One, IE_Released, this, &AWowGameplayController::OnSpellKey1);
	InputComponent->BindKey(EKeys::Two, IE_Released, this, &AWowGameplayController::OnSpellKey2);
	InputComponent->BindKey(EKeys::Three, IE_Released, this, &AWowGameplayController::OnSpellKey3);
	InputComponent->BindKey(EKeys::Four, IE_Released, this, &AWowGameplayController::OnSpellKey4);
	InputComponent->BindKey(EKeys::Five, IE_Released, this, &AWowGameplayController::OnSpellKey5);
	InputComponent->BindKey(EKeys::Six, IE_Released, this, &AWowGameplayController::OnSpellKey6);
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

	// Bind combat events
	ConnectionManager->PacketHandler.OnSpellStart.AddUObject(
		this, &AWowGameplayController::OnSpellStart);
	ConnectionManager->PacketHandler.OnSpellGo.AddUObject(
		this, &AWowGameplayController::OnSpellGo);
	ConnectionManager->PacketHandler.OnSpellFailure.AddUObject(
		this, &AWowGameplayController::OnSpellFailure);
	ConnectionManager->PacketHandler.OnAttackerStateUpdate.AddUObject(
		this, &AWowGameplayController::OnAttackerStateUpdate);

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

	// Update cast bar progress
	if (bIsCasting && CastBarWidget.IsValid())
	{
		float ElapsedTime = GetWorld()->GetTimeSeconds() - CastStartTime;
		float Progress = FMath::Clamp(ElapsedTime / CastDuration, 0.0f, 1.0f);
		CastBarWidget->UpdateProgress(Progress);
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

// ── Combat System ───────────────────────────────────────────────────────

void AWowGameplayController::CastSpell(int32 SpellId)
{
	if (!ConnectionManager) return;

	// Don't cast if already casting
	if (bIsCasting)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Already casting spell %u"), CurrentSpellId);
		return;
	}

	int64 TargetGuidSigned = static_cast<int64>(TargetGuid);
	ConnectionManager->SendCastSpell(SpellId, TargetGuidSigned);

	UE_LOG(LogWowGameplay, Log, TEXT("Cast spell %u on target %llu"), SpellId, TargetGuid);
}

void AWowGameplayController::StartAutoAttack()
{
	if (!ConnectionManager || TargetGuid == 0) return;

	// Stop any existing auto-attack
	if (bIsAutoAttacking && AutoAttackTargetGuid != TargetGuid)
	{
		StopAutoAttack();
	}

	// Start auto-attack on new target
	if (!bIsAutoAttacking)
	{
		bIsAutoAttacking = true;
		AutoAttackTargetGuid = TargetGuid;
		ConnectionManager->SendAttackSwing(static_cast<int64>(TargetGuid));
		UE_LOG(LogWowGameplay, Log, TEXT("Started auto-attack on target %llu"), TargetGuid);
	}
}

void AWowGameplayController::StopAutoAttack()
{
	if (!ConnectionManager || !bIsAutoAttacking) return;

	bIsAutoAttacking = false;
	AutoAttackTargetGuid = 0;
	ConnectionManager->SendAttackStop();
	UE_LOG(LogWowGameplay, Log, TEXT("Stopped auto-attack"));
}

void AWowGameplayController::OnRightClick()
{
	// If we have a hostile target, start auto-attack
	if (TargetGuid != 0)
	{
		// Check if target is hostile (simplified check for now)
		const FWowEntity* TargetEntity = nullptr;
		if (ConnectionManager)
		{
			TargetEntity = ConnectionManager->PacketHandler.EntityManager.Find(TargetGuid);
		}

		if (TargetEntity && TargetEntity->IsUnit())
		{
			// For now, assume all NPCs are hostile (in a real implementation, check faction)
			if (!TargetEntity->IsPlayer())
			{
				StartAutoAttack();
			}
		}
	}
	else
	{
		// No target - try to target something under cursor first
		TryTargetUnderCursor();
		// If we successfully targeted something hostile, start auto-attack
		if (TargetGuid != 0)
		{
			OnRightClick(); // Recursive call to handle the new target
		}
	}
}

void AWowGameplayController::OnSpellStart(uint64 CasterGuid, uint32 SpellId, uint32 CastFlags, int32 CastTime)
{
	if (!ConnectionManager) return;

	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;

	if (CasterGuid == LocalGuid)
	{
		// We're casting a spell
		bIsCasting = true;
		CurrentSpellId = SpellId;
		CastStartTime = GetWorld()->GetTimeSeconds();
		CastDuration = CastTime / 1000.0f; // Convert milliseconds to seconds

		// Get spell name from DBC
		FString SpellName = FString::Printf(TEXT("Spell %u"), SpellId);
		const FDbcStore& DbcStore = FDbcStore::Get();
		if (DbcStore.IsLoaded())
		{
			if (const auto* SpellEntry = DbcStore.Spells().GetById(SpellId))
			{
				SpellName = SpellEntry->SpellName;
			}
		}

		UE_LOG(LogWowGameplay, Log, TEXT("Started casting %s (%.1fs cast time)"), *SpellName, CastDuration);

		// Show cast bar if cast time > 0
		if (CastBarWidget.IsValid() && CastTime > 0)
		{
			CastBarWidget->StartCast(SpellName, CastDuration);
		}
	}
	else
	{
		UE_LOG(LogWowGameplay, Log, TEXT("Entity %llu started casting spell %u"), CasterGuid, SpellId);
	}
}

void AWowGameplayController::OnSpellGo(uint64 CasterGuid, uint32 SpellId, uint32 CastFlags)
{
	if (!ConnectionManager) return;

	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;

	if (CasterGuid == LocalGuid)
	{
		// Our spell cast completed
		bIsCasting = false;
		CurrentSpellId = 0;
		CastStartTime = 0.0f;
		CastDuration = 0.0f;

		UE_LOG(LogWowGameplay, Log, TEXT("Completed casting spell %u"), SpellId);

		// Hide cast bar widget
		if (CastBarWidget.IsValid())
		{
			CastBarWidget->StopCast();
		}
	}
	else
	{
		UE_LOG(LogWowGameplay, Log, TEXT("Entity %llu completed casting spell %u"), CasterGuid, SpellId);
	}
}

void AWowGameplayController::OnSpellFailure(uint64 CasterGuid, uint32 SpellId, uint8 FailureReason)
{
	if (!ConnectionManager) return;

	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;

	if (CasterGuid == LocalGuid)
	{
		// Our spell cast failed
		bIsCasting = false;
		CurrentSpellId = 0;
		CastStartTime = 0.0f;
		CastDuration = 0.0f;

		UE_LOG(LogWowGameplay, Warning, TEXT("Spell %u failed with reason %u"), SpellId, FailureReason);

		// Hide cast bar widget
		if (CastBarWidget.IsValid())
		{
			CastBarWidget->StopCast();
		}

		// TODO: Show error message based on failure reason
	}
	else
	{
		UE_LOG(LogWowGameplay, Log, TEXT("Entity %llu spell %u failed"), CasterGuid, SpellId);
	}
}

void AWowGameplayController::OnAttackerStateUpdate(uint64 AttackerGuid, uint64 VictimGuid, uint32 HitInfo, uint32 Damage)
{
	if (!ConnectionManager) return;

	UE_LOG(LogWowGameplay, Log, TEXT("Combat: Attacker %llu hit target %llu for %u damage (hitInfo=0x%08X)"),
		AttackerGuid, VictimGuid, Damage, HitInfo);

	// Show floating combat text
	TObjectPtr<AActor>* TargetActorPtr = SpawnedEntityActors.Find(VictimGuid);
	if (TargetActorPtr && *TargetActorPtr && Damage > 0)
	{
		FWowFloatingTextInfo TextInfo;
		TextInfo.Text = FString::Printf(TEXT("%u"), Damage);
		TextInfo.Duration = 2.0f;
		TextInfo.Speed = 100.0f;
		TextInfo.FontSize = 32.0f;

		// Color based on hit info (simplified)
		const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;
		if (AttackerGuid == LocalGuid)
		{
			// Our damage - white for physical, colored for schools
			TextInfo.Color = FLinearColor::White;
		}
		else if (VictimGuid == LocalGuid)
		{
			// Damage to us - red
			TextInfo.Color = FLinearColor::Red;
		}
		else
		{
			// Other damage - gray
			TextInfo.Color = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
		}

		// Check for critical hit (bit 1 in HitInfo)
		if (HitInfo & 0x2)
		{
			TextInfo.Text += TEXT("!");
			TextInfo.Color = FLinearColor::Yellow;
			TextInfo.FontSize = 40.0f;
		}

		UWowFloatingText::SpawnFloatingText(*TargetActorPtr, TextInfo);

		UE_LOG(LogWowGameplay, Log, TEXT("Spawned floating damage text: %s"), *TextInfo.Text);
	}

	// Check if our auto-attack target died (simplified check)
	if (bIsAutoAttacking && VictimGuid == AutoAttackTargetGuid)
	{
		// In a real implementation, we'd check if target health reached 0
		// For now, we'll keep auto-attacking until manually stopped
	}
}
