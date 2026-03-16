#include "WowGameplayController.h"
#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "WowEntity.h"
#include "WowOpcodes.h"
#include "WowPlayerCharacter.h"
#include "WowUIManager.h"
#include "WowGameUI.h"
#include "WowEventSystem.h"
#include "WowWorldManager.h"
#include "WowCharacterBuilder.h"
#include "WowAnimationController.h"
#include "WowNameplateWidget.h"
#include "SWowCombatLog.h"
#include "SWowChatWindow.h"
#include "SWowPartyFrame.h"
#include "SWowPartyInvite.h"
#include "SWowTaxiMap.h"
#include "WowDeathManager.h"
#include "WowCursorManager.h"
#include "WowTooltipManager.h"
#include "UI/SWowActionBar.h"
#include "UI/SWowMinimap.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Coord/WowCoordinate.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Widgets/SViewport.h"
#include "SWowCastBar.h"
#include "WowFloatingText.h"
#include "Formats/Dbc/DbcStore.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowGameplay, Log, All);

AWowGameplayController::AWowGameplayController()
{
	bShowMouseCursor = false;
	PrimaryActorTick.bCanEverTick = true;

	// Create Game UI component
	GameUI = CreateDefaultSubobject<UWowGameUI>(TEXT("WowGameUI"));
}

void AWowGameplayController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameOnly());

	// Cache UIManager early so OnUpdate ticks even without networking
	if (UGameInstance* GI = GetGameInstance())
	{
		UIManager = GI->GetSubsystem<UWowUIManager>();
		if (UIManager)
		{
			// Initialize inventory UI overlays
			UIManager->SetRootCanvas(nullptr); // We'll use viewport overlay instead
		}
	}

	// Create managers
	DeathManager = NewObject<UWowDeathManager>(this);
	CursorManager = NewObject<UWowCursorManager>(this);
	TooltipManager = NewObject<UWowTooltipManager>(this);

	// Create cast bar widget wrapped in positioning container
	TSharedRef<SWidget> CastBarContainer =
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible) // Pass clicks through to 3D world
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)  // Bottom center positioning
		.VAlign(VAlign_Bottom)
		.Padding(0, 0, 0, 80)  // 80px margin from bottom (above action bar)
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

	// Action bar keybinds - use IE_Released to avoid conflicts and match WoW behavior
	InputComponent->BindKey(EKeys::One, IE_Released, this, &AWowGameplayController::OnActionSlot1);
	InputComponent->BindKey(EKeys::Two, IE_Released, this, &AWowGameplayController::OnActionSlot2);
	InputComponent->BindKey(EKeys::Three, IE_Released, this, &AWowGameplayController::OnActionSlot3);
	InputComponent->BindKey(EKeys::Four, IE_Released, this, &AWowGameplayController::OnActionSlot4);
	InputComponent->BindKey(EKeys::Five, IE_Released, this, &AWowGameplayController::OnActionSlot5);
	InputComponent->BindKey(EKeys::Six, IE_Released, this, &AWowGameplayController::OnActionSlot6);
	InputComponent->BindKey(EKeys::Seven, IE_Released, this, &AWowGameplayController::OnActionSlot7);
	InputComponent->BindKey(EKeys::Eight, IE_Released, this, &AWowGameplayController::OnActionSlot8);
	InputComponent->BindKey(EKeys::Nine, IE_Released, this, &AWowGameplayController::OnActionSlot9);
	InputComponent->BindKey(EKeys::Zero, IE_Released, this, &AWowGameplayController::OnActionSlot0);
	InputComponent->BindKey(EKeys::Hyphen, IE_Released, this, &AWowGameplayController::OnActionSlotMinus);
	InputComponent->BindKey(EKeys::Equals, IE_Released, this, &AWowGameplayController::OnActionSlotEquals);

	// Minimap toggle
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AWowGameplayController::OnToggleMap);

	// Chat input
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AWowGameplayController::OnEnterKey);

	// Inventory UI keys
	InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AWowGameplayController::OnBagKey);
	InputComponent->BindKey(EKeys::C, IE_Pressed, this, &AWowGameplayController::OnCharacterKey);
}

void AWowGameplayController::BindEntityEvents()
{
	if (!ConnectionManager) return;

	// Listen for login verify world — teleport pawn to spawn position
	ConnectionManager->PacketHandler.OnLoginVerifyWorld.AddUObject(
		this, &AWowGameplayController::OnLoginVerifyWorld);

	// Listen for teleport requests from server
	ConnectionManager->PacketHandler.OnTeleportRequest.AddUObject(
		this, &AWowGameplayController::OnTeleportRequest);

	// Listen for map transfers
	ConnectionManager->PacketHandler.OnMapTransfer.AddUObject(
		this, &AWowGameplayController::OnMapTransfer);

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

	// Listen for player inventory updates
	ConnectionManager->PacketHandler.OnPlayerInventoryUpdate.AddUObject(
		this, &AWowGameplayController::OnPlayerInventoryUpdated);

	// Bind combat log events
	ConnectionManager->PacketHandler.OnSpellStart.AddUObject(
		this, &AWowGameplayController::OnSpellStart);
	ConnectionManager->PacketHandler.OnSpellGo.AddUObject(
		this, &AWowGameplayController::OnSpellGo);
	ConnectionManager->PacketHandler.OnSpellFailure.AddUObject(
		this, &AWowGameplayController::OnSpellFailure);
	ConnectionManager->PacketHandler.OnAttackerStateUpdate.AddUObject(
		this, &AWowGameplayController::OnAttackerStateUpdate);
	ConnectionManager->PacketHandler.OnChatMessage.AddUObject(
		this, &AWowGameplayController::OnChatMessage);

	// Bind party/group events
	ConnectionManager->PacketHandler.OnGroupUpdated.AddUObject(
		this, &AWowGameplayController::OnGroupUpdated);
	ConnectionManager->PacketHandler.OnGroupInviteReceived.AddUObject(
		this, &AWowGameplayController::OnGroupInviteReceived);
	ConnectionManager->PacketHandler.OnPartyCommandResult.AddUObject(
		this, &AWowGameplayController::OnPartyCommandResult);

	// Bind taxi events
	ConnectionManager->PacketHandler.OnTaxiNodesShown.AddUObject(
		this, &AWowGameplayController::OnTaxiNodesShown);
	ConnectionManager->PacketHandler.OnTaxiActivateReply.AddUObject(
		this, &AWowGameplayController::OnTaxiActivateReply);

	// Listen for name query responses
	ConnectionManager->PacketHandler.OnPlayerNameReceived.AddUObject(
		this, &AWowGameplayController::OnPlayerNameReceived);
	ConnectionManager->PacketHandler.OnCreatureNameReceived.AddUObject(
		this, &AWowGameplayController::OnCreatureNameReceived);

	// Listen for action button updates to refresh action bar
	ConnectionManager->PacketHandler.OnOpcodeReceived.AddLambda([this](uint16 Opcode)
	{
		if (Opcode == WowOpcode::SMSG_ACTION_BUTTONS && ActionBarWidget.IsValid())
		{
			ActionBarWidget->RefreshActionButtons();
		}
	});

	// Bind game UI events
	ConnectionManager->PacketHandler.OnLootOpened.AddLambda(
		[this](uint64 LootGuid, uint8 LootType, uint32 Gold, const TArray<FWowLootItem>& Items)
		{
			if (GameUI)
			{
				GameUI->ShowLootWindow(LootGuid, LootType, Gold, Items);
			}
		});

	ConnectionManager->PacketHandler.OnLootClosed.AddLambda(
		[this]()
		{
			if (GameUI)
			{
				GameUI->HideLootWindow();
			}
		});

	ConnectionManager->PacketHandler.OnVendorOpened.AddLambda(
		[this](uint64 VendorGuid, const TArray<FWowVendorItem>& Items)
		{
			if (GameUI)
			{
				GameUI->ShowVendorWindow(VendorGuid, Items);
			}
		});

	ConnectionManager->PacketHandler.OnQuestDialog.AddLambda(
		[this](const FWowQuestDetails& QuestDetails)
		{
			if (GameUI)
			{
				GameUI->ShowQuestDialog(QuestDetails);
			}
		});

	ConnectionManager->PacketHandler.OnQuestRewardDialog.AddLambda(
		[this](const FWowQuestDetails& QuestDetails)
		{
			if (GameUI)
			{
				GameUI->ShowQuestRewardDialog(QuestDetails);
			}
		});

	// Setup GameUI connection manager
	if (GameUI)
	{
		GameUI->SetConnectionManager(ConnectionManager);
	}

	// Cache UIManager for tick dispatch
	if (UGameInstance* GI = GetGameInstance())
	{
		UIManager = GI->GetSubsystem<UWowUIManager>();
	}

	// Initialize managers once connection is established
	InitializeManagers();
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

	// Create action bar widget when entering the world
	CreateActionBarWidget();

	// Create minimap widget
	CreateMinimapWidget();
}

void AWowGameplayController::OnEntityUpdated(const FWowEntity& Entity)
{
	if (!ConnectionManager) return;

	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;

	// Track health changes for combat log
	static TMap<uint64, int32> LastKnownHealth;
	int32 CurrentHealth = Entity.GetHealth();
	int32* OldHealthPtr = LastKnownHealth.Find(Entity.Guid);
	if (OldHealthPtr && *OldHealthPtr != CurrentHealth && CurrentHealth > 0)
	{
		OnEntityHealthChanged(Entity, *OldHealthPtr, CurrentHealth);
	}
	if (CurrentHealth > 0)
	{
		LastKnownHealth.Add(Entity.Guid, CurrentHealth);
	}

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

	// Update party frame with entity data for group members
	if (PartyFrameWidget.IsValid() && Entity.IsUnit())
	{
		PartyFrameWidget->UpdateMemberFromEntity(Entity);
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

	// Update tooltip manager
	if (TooltipManager && FSlateApplication::IsInitialized())
	{
		FVector2D MousePosition = FSlateApplication::Get().GetCursorPos();
		TooltipManager->Update(MousePosition);
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

	// Update character animations
	UpdatePlayerAnimations();
	UpdateEntityAnimations();

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

	// Handle local player differently - apply character model to existing pawn
	if (Entity.Guid == ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid)
	{
		SetupLocalPlayerCharacterModel(Entity);
		return;
	}

	// Query names for newly created entities
	if (Entity.IsPlayer())
	{
		// Check if we already have the name
		if (!ConnectionManager->PacketHandler.PlayerNameCache.Contains(Entity.Guid))
		{
			ConnectionManager->SendNameQuery(Entity.Guid);
		}
	}
	else if (Entity.Entry > 0) // Creature or GameObject
	{
		// Check if we already have the creature name
		if (!ConnectionManager->PacketHandler.CreatureNameCache.Contains(Entity.Entry))
		{
			ConnectionManager->SendCreatureQuery(Entity.Entry, Entity.Guid);
		}
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

void AWowGameplayController::OnTeleportRequest(uint64 Guid, uint32 Flags, uint32 Time, FVector Position, float Orientation)
{
	if (!ConnectionManager) return;

	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;

	// Only handle teleports for our local player
	if (Guid != LocalGuid)
	{
		UE_LOG(LogWowGameplay, Verbose, TEXT("Ignoring teleport for non-local entity GUID %llu"), Guid);
		return;
	}

	// Convert from WoW coordinates to UE coordinates
	FVector UEPosition = FWowCoordinate::WowToUE(Position);

	// Teleport the local pawn
	APawn* LocalPawn = GetPawn();
	if (LocalPawn)
	{
		LocalPawn->SetActorLocation(UEPosition, false, nullptr, ETeleportType::TeleportPhysics);
		LocalPawn->SetActorRotation(FRotator(0.0f, FMath::RadiansToDegrees(Orientation), 0.0f));

		if (APlayerController* PC = Cast<APlayerController>(LocalPawn->GetController()))
		{
			PC->SetControlRotation(FRotator(0.0f, FMath::RadiansToDegrees(Orientation), 0.0f));
		}

		UE_LOG(LogWowGameplay, Log, TEXT("Teleported to: WoW=(%.1f,%.1f,%.1f) UE=(%.0f,%.0f,%.0f) orient=%.2f"),
			Position.X, Position.Y, Position.Z, UEPosition.X, UEPosition.Y, UEPosition.Z, Orientation);

		// Update server position tracking
		bHasServerPosition = true;
		LastServerPosition = UEPosition;
	}

	// Send acknowledgment to server
	SendTeleportAck(Guid, Flags, Time);
}

void AWowGameplayController::OnMapTransfer(uint32 MapId, float X, float Y, float Z, float Orientation)
{
	UE_LOG(LogWowGameplay, Log, TEXT("Map transfer to: map=%u pos=(%.1f,%.1f,%.1f) orient=%.2f"),
		MapId, X, Y, Z, Orientation);

	// For basic implementation, treat this like a teleport to new position
	FVector Position(X, Y, Z);
	FVector UEPosition = FWowCoordinate::WowToUE(Position);

	// Teleport the local pawn
	APawn* LocalPawn = GetPawn();
	if (LocalPawn)
	{
		LocalPawn->SetActorLocation(UEPosition, false, nullptr, ETeleportType::TeleportPhysics);
		LocalPawn->SetActorRotation(FRotator(0.0f, FMath::RadiansToDegrees(Orientation), 0.0f));

		if (APlayerController* PC = Cast<APlayerController>(LocalPawn->GetController()))
		{
			PC->SetControlRotation(FRotator(0.0f, FMath::RadiansToDegrees(Orientation), 0.0f));
		}

		// Update server position tracking
		bHasServerPosition = true;
		LastServerPosition = UEPosition;
	}

	// Send worldport acknowledgment
	SendWorldportAck();

	// TODO: In a full implementation, this would:
	// 1. Clear all existing entities
	// 2. Load the new map's terrain and doodads
	// 3. Wait for SMSG_LOGIN_VERIFY_WORLD for the new map
}

void AWowGameplayController::SendTeleportAck(uint64 Guid, uint32 Flags, uint32 Time)
{
	if (!ConnectionManager) return;

	// CMSG_MOVE_TELEPORT_ACK: uint64 guid + uint32 flags + uint32 time
	TArray<uint8> Data;
	Data.SetNumUninitialized(16); // 8 + 4 + 4

	FMemory::Memcpy(Data.GetData(), &Guid, 8);
	FMemory::Memcpy(Data.GetData() + 8, &Flags, 4);
	FMemory::Memcpy(Data.GetData() + 12, &Time, 4);

	ConnectionManager->SendRawPacket(WowOpcode::MSG_MOVE_TELEPORT_ACK, Data);

	UE_LOG(LogWowGameplay, Log, TEXT("Sent CMSG_MOVE_TELEPORT_ACK: guid=%llu flags=0x%08X time=%u"),
		Guid, Flags, Time);
}

void AWowGameplayController::SendWorldportAck()
{
	if (!ConnectionManager) return;

	// MSG_MOVE_WORLDPORT_ACK has no payload
	ConnectionManager->SendRawPacket(WowOpcode::MSG_MOVE_WORLDPORT_ACK, {});

	UE_LOG(LogWowGameplay, Log, TEXT("Sent MSG_MOVE_WORLDPORT_ACK"));
}

void AWowGameplayController::AddCombatMessage(const FString& Message, const FLinearColor& Color)
{
	// Forward to chat window (combat tab) if available, otherwise use legacy combat log
	if (ChatWindow.IsValid())
	{
		ChatWindow->AddCombatMessage(Message, Color);
	}
	else if (CombatLog.IsValid())
	{
		CombatLog->AddCombatMessage(Message, Color);
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

	if (CasterGuid == LocalGuid)
	{
		// We're casting a spell - set up cast bar and state
		bIsCasting = true;
		CurrentSpellId = SpellId;
		CastStartTime = GetWorld()->GetTimeSeconds();
		CastDuration = CastTime / 1000.0f; // Convert milliseconds to seconds

		UE_LOG(LogWowGameplay, Log, TEXT("Started casting %s (%.1fs cast time)"), *SpellName, CastDuration);

		// Show cast bar if cast time > 0
		if (CastBarWidget.IsValid() && CastTime > 0)
		{
			CastBarWidget->StartCast(SpellName, CastDuration);
		}

		// Also log to combat log
		FString Message = FString::Printf(TEXT("You cast %s"), *SpellName);
		AddCombatMessage(Message, FLinearColor::Yellow);
	}
	else
	{
		// Other entity casting
		UE_LOG(LogWowGameplay, Log, TEXT("Entity %llu started casting spell %u"), CasterGuid, SpellId);

		// Log to combat log
		FString CasterName = FString::Printf(TEXT("Entity %llu"), CasterGuid);
		FString Message = FString::Printf(TEXT("%s cast %s"), *CasterName, *SpellName);
		AddCombatMessage(Message, FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));
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
		UE_LOG(LogWowGameplay, Log, TEXT("Entity %llu completed spell %u"), CasterGuid, SpellId);
	}
}

void AWowGameplayController::InitializeManagers()
{
	if (!ConnectionManager)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Cannot initialize managers: ConnectionManager is null"));
		return;
	}

	CacheWorldResources();

	// TODO: Get root widget for UI overlay
	// In a real implementation, this would get the viewport widget or main UI overlay
	TSharedPtr<SWidget> RootWidget; // Placeholder

	// Initialize death manager
	if (DeathManager)
	{
		DeathManager->Initialize(ConnectionManager, RootWidget);
	}

	// Initialize cursor manager
	if (CursorManager && CachedMpq && CachedAssetCache)
	{
		CursorManager->Initialize(CachedMpq, CachedAssetCache);
		CursorManager->LoadCursors();
	}

	// Initialize tooltip manager
	if (TooltipManager)
	{
		TooltipManager->Initialize(ConnectionManager, RootWidget);
	}

	UE_LOG(LogWowGameplay, Log, TEXT("All managers initialized"));
}

void AWowGameplayController::SetupLocalPlayerCharacterModel(const FWowEntity& Entity)
{
	CacheWorldResources();
	if (!CachedMpq || !CachedAssetCache)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Cannot setup local player model - missing world resources. Retrying later..."));

		// Try again in 1 second - the world manager might not be ready yet
		FTimerHandle RetryHandle;
		GetWorldTimerManager().SetTimer(
			RetryHandle,
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

void AWowGameplayController::UpdatePlayerAnimations()
{
	// Update local player animation based on character movement
	if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn()))
	{
		// If the player has an animation controller, update it
		if (UWowAnimationController* AnimController = PlayerChar->AnimationController)
		{
			AnimController->UpdateLocalPlayerState(PlayerChar);
		}
	}
}

void AWowGameplayController::UpdateEntityAnimations()
{
	if (!ConnectionManager) return;

	// Update animations for all spawned entity actors based on their movement state
	for (auto& Pair : SpawnedEntityActors)
	{
		uint64 Guid = Pair.Key;
		AActor* Actor = Pair.Value;

		if (!Actor || !IsValid(Actor)) continue;

		// Get the animation controller for this actor
		UWowAnimationController* AnimController = FWowCharacterBuilder::GetAnimationController(Actor);
		if (!AnimController) continue;

		// Get the entity data to determine animation state
		const FWowEntity* Entity = ConnectionManager->PacketHandler.EntityManager.Find(Guid);
		if (!Entity) continue;

		// Check if entity is moving via spline (NPC pathing) or position change
		bool bIsOnSpline = Entity->Movement.bHasActiveSpline;

		// If on spline, force walk/run animation by setting a synthetic movement flag
		FWowMovementInfo AnimMovement = Entity->Movement;
		if (bIsOnSpline)
		{
			AnimMovement.MoveFlags |= WowMovementFlags::FORWARD;
		}

		// Also detect movement by checking if the actor is actually moving
		if (!bIsOnSpline && !(AnimMovement.MoveFlags & (WowMovementFlags::FORWARD | WowMovementFlags::BACKWARD)))
		{
			FVector Velocity = Actor->GetVelocity();
			if (Velocity.SizeSquared() > 100.0f) // Moving faster than 10 cm/s
			{
				AnimMovement.MoveFlags |= WowMovementFlags::FORWARD;
			}
		}

		bool bEntityInCombat = false;
		bool bEntityCasting = false;

		AnimController->UpdateAnimationState(AnimMovement, bEntityInCombat, bEntityCasting);
	}
}

void AWowGameplayController::CreateActionBarWidget()
{
	if (!ConnectionManager || ActionBarWidget.IsValid())
	{
		return;
	}

	// Create the action bar widget
	ActionBarWidget = SNew(SWowActionBar)
		.ConnectionManager(ConnectionManager);

	// Add to viewport with proper positioning container
	if (GEngine && GEngine->GameViewport)
	{
		// Wrap in positioned overlay that passes clicks through
		TSharedRef<SWidget> ActionBarContainer =
			SNew(SOverlay)
			.Visibility(EVisibility::SelfHitTestInvisible) // Pass clicks through to 3D world
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)  // Bottom center positioning
			.VAlign(VAlign_Bottom)
			.Padding(0, 0, 0, 20)  // 20px margin from bottom edge
			[
				ActionBarWidget.ToSharedRef()
			];

		GEngine->GameViewport->AddViewportWidgetContent(
			ActionBarContainer,
			60 // Z-order
		);
	}

	UE_LOG(LogWowGameplay, Log, TEXT("Created action bar widget"));
}

void AWowGameplayController::CreateMinimapWidget()
{
	if (!ConnectionManager || MinimapWidget.IsValid())
	{
		return;
	}

	// Get world manager
	AWowWorldManager* WorldManager = nullptr;
	if (UWorld* World = GetWorld())
	{
		TArray<AActor*> WorldManagers;
		UGameplayStatics::GetAllActorsOfClass(World, AWowWorldManager::StaticClass(), WorldManagers);
		if (WorldManagers.Num() > 0)
		{
			WorldManager = Cast<AWowWorldManager>(WorldManagers[0]);
		}
	}

	if (!WorldManager)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Could not find WorldManager for minimap"));
		return;
	}

	// Create the minimap widget
	MinimapWidget = SNew(SWowMinimap)
		.EntityManager(&ConnectionManager->PacketHandler.EntityManager)
		.WorldManager(WorldManager);

	// Add to viewport with proper positioning container
	if (GEngine && GEngine->GameViewport)
	{
		// Wrap in positioned overlay that passes clicks through
		TSharedRef<SWidget> MinimapContainer =
			SNew(SOverlay)
			.Visibility(EVisibility::SelfHitTestInvisible) // Pass clicks through to 3D world
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)  // Top right positioning
			.VAlign(VAlign_Top)
			.Padding(0, 20, 20, 0)  // 20px margin from top and right edges
			[
				MinimapWidget.ToSharedRef()
			];

		GEngine->GameViewport->AddViewportWidgetContent(
			MinimapContainer,
			70 // Z-order (above action bar)
		);
	}

	UE_LOG(LogWowGameplay, Log, TEXT("Created minimap widget"));
}

void AWowGameplayController::CreatePartyFrame()
{
	if (!ConnectionManager || PartyFrameWidget.IsValid())
	{
		return;
	}

	// Create the party frame widget
	PartyFrameWidget = SNew(SWowPartyFrame)
		.ConnectionManager(ConnectionManager);

	// Add to viewport with proper positioning container
	if (GEngine && GEngine->GameViewport)
	{
		// Wrap in positioned overlay that passes clicks through
		TSharedRef<SWidget> PartyFrameContainer =
			SNew(SOverlay)
			.Visibility(EVisibility::SelfHitTestInvisible) // Pass clicks through to 3D world
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)   // Top left positioning
			.VAlign(VAlign_Top)
			.Padding(20, 100, 0, 0)  // 20px from left, 100px from top (below minimap area)
			[
				PartyFrameWidget.ToSharedRef()
			];

		GEngine->GameViewport->AddViewportWidgetContent(
			PartyFrameContainer,
			50 // Z-order
		);
	}

	UE_LOG(LogWowGameplay, Log, TEXT("Created party frame widget"));
}

void AWowGameplayController::OnGroupUpdated()
{
	if (!ConnectionManager)
	{
		return;
	}

	// Create party frame if needed
	if (!PartyFrameWidget.IsValid())
	{
		CreatePartyFrame();
	}

	// Update party frame with new group data
	if (PartyFrameWidget.IsValid())
	{
		PartyFrameWidget->UpdatePartyInfo(ConnectionManager->PacketHandler.GroupInfo);
	}

	UE_LOG(LogWowGameplay, Log, TEXT("Group updated"));
}

void AWowGameplayController::OnGroupInviteReceived(const FString& InviterName)
{
	UE_LOG(LogWowGameplay, Log, TEXT("Group invite received from %s"), *InviterName);

	// Close any existing invite dialog
	if (PartyInviteWidget.IsValid())
	{
		// For party invites, the widget manages its own lifecycle and will auto-remove
		PartyInviteWidget.Reset();
	}

	// Create and show new invite dialog
	PartyInviteWidget = SNew(SWowPartyInvite)
		.ConnectionManager(ConnectionManager)
		.InviterName(InviterName);

	if (GEngine && GEngine->GameViewport)
	{
		// Wrap in positioned overlay for center positioning
		TSharedRef<SWidget> PartyInviteContainer =
			SNew(SOverlay)
			.Visibility(EVisibility::SelfHitTestInvisible) // Pass clicks through to 3D world (except the widget itself)
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)  // Center positioning
			.VAlign(VAlign_Center)
			[
				PartyInviteWidget.ToSharedRef()
			];

		GEngine->GameViewport->AddViewportWidgetContent(
			PartyInviteContainer,
			100 // High Z-order to appear on top
		);
	}
}

void AWowGameplayController::OnPartyCommandResult(uint8 Command, const FString& PlayerName, uint8 Result)
{
	// Log the result and potentially show a message
	const TCHAR* CommandName = TEXT("Unknown");
	switch (Command)
	{
	case 0: CommandName = TEXT("Invite"); break;
	case 1: CommandName = TEXT("Leave"); break;
	case 2: CommandName = TEXT("Remove"); break;
	default: break;
	}

	const TCHAR* ResultName = TEXT("Unknown");
	switch (Result)
	{
	case 0: ResultName = TEXT("Success"); break;
	case 1: ResultName = TEXT("Cannot find player"); break;
	case 2: ResultName = TEXT("Player not in your group"); break;
	case 3: ResultName = TEXT("Player already in group"); break;
	default: break;
	}

	UE_LOG(LogWowGameplay, Log, TEXT("Party command result: %s for %s - %s"),
		CommandName, *PlayerName, ResultName);

	// Add message to chat window
	if (ChatWindow.IsValid())
	{
		FString Message = FString::Printf(TEXT("%s %s: %s"), CommandName, *PlayerName, ResultName);
		ChatWindow->AddCombatMessage(Message, FLinearColor::Yellow);
	}
}

void AWowGameplayController::OnTaxiNodesShown(uint64 NpcGuid, uint32 CurrentNodeId)
{
	UE_LOG(LogWowGameplay, Log, TEXT("Taxi nodes shown by NPC %llu, current node: %d"),
		NpcGuid, CurrentNodeId);

	// Create taxi map widget if needed
	if (!TaxiMapWidget.IsValid())
	{
		TaxiMapWidget = SNew(SWowTaxiMap)
			.ConnectionManager(ConnectionManager);

		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->AddViewportWidgetContent(
				TaxiMapWidget.ToSharedRef(),
				200 // Very high Z-order for full screen overlay
			);
		}
	}

	// Show the taxi map with current data
	if (TaxiMapWidget.IsValid())
	{
		TaxiMapWidget->ShowTaxiMap(ConnectionManager->PacketHandler.TaxiData);
	}
}

void AWowGameplayController::OnTaxiActivateReply(uint8 Result)
{
	const TCHAR* ResultName = TEXT("Unknown");
	switch (Result)
	{
	case 0: ResultName = TEXT("Success"); break;
	case 1: ResultName = TEXT("Unspecified error"); break;
	case 2: ResultName = TEXT("No such path"); break;
	case 3: ResultName = TEXT("Not enough money"); break;
	case 4: ResultName = TEXT("Too far away"); break;
	case 5: ResultName = TEXT("No vendor nearby"); break;
	default: break;
	}

	UE_LOG(LogWowGameplay, Log, TEXT("Taxi activate result: %s"), ResultName);

	if (Result != 0)
	{
		// Show error message
		if (ChatWindow.IsValid())
		{
			FString Message = FString::Printf(TEXT("Cannot use taxi: %s"), ResultName);
			ChatWindow->AddCombatMessage(Message, FLinearColor::Red);
		}
	}
	else
	{
		// Success - player should start flying
		// Close taxi map
		if (TaxiMapWidget.IsValid())
		{
			TaxiMapWidget->CloseTaxiMap();
		}

		// Add flight message
		if (ChatWindow.IsValid())
		{
			ChatWindow->AddCombatMessage(TEXT("You are now on a flight path"), FLinearColor::Green);
		}
	}
}

void AWowGameplayController::OnToggleMap()
{
	if (MinimapWidget.IsValid())
	{
		MinimapWidget->ToggleFullScreenMap();
	}
}

void AWowGameplayController::CastSpellFromSlot(int32 SlotIndex)
{
	if (!ConnectionManager || !ConnectionManager->PacketHandler.ActionButtons.IsValidIndex(SlotIndex))
	{
		return;
	}

	uint32 PackedAction = ConnectionManager->PacketHandler.ActionButtons[SlotIndex];
	uint32 ActionId = PackedAction & 0x00FFFFFF;
	uint8 ActionType = (PackedAction >> 24) & 0xFF;

	// Only handle spell actions (type 0)
	if (ActionType == 0 && ActionId > 0)
	{
		ConnectionManager->SendCastSpell(ActionId);
		UE_LOG(LogWowGameplay, Log, TEXT("Cast spell %u from slot %d (key pressed)"), ActionId, SlotIndex);
	}
}

void AWowGameplayController::OnPlayerNameReceived(uint64 Guid, const FString& Name)
{
	// TODO: Update nameplate for matching entity
	UE_LOG(LogWowGameplay, Log, TEXT("Received player name: GUID=%llu Name=%s"), Guid, *Name);
}

void AWowGameplayController::OnCreatureNameReceived(uint32 Entry, const FString& Name, const FString& Title)
{
	// TODO: Update nameplate for matching entities
	UE_LOG(LogWowGameplay, Log, TEXT("Received creature name: Entry=%u Name=%s Title=%s"), Entry, *Name, *Title);
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


void AWowGameplayController::OnChatMessage(uint8 Type, uint32 Language, uint64 SenderGuid, const FString& SenderName, const FString& Message, const FString& Channel)
{
	// Forward to chat window if available
	if (ChatWindow.IsValid())
	{
		ChatWindow->AddChatMessage(Type, Language, SenderGuid, SenderName, Message, Channel);
	}
	else
	{
		// Fallback to combat log for now
		AddCombatMessage(Message, FLinearColor::White);
	}
}

void AWowGameplayController::OnEntityHealthChanged(const FWowEntity& Entity, int32 OldHealth, int32 NewHealth)
{
	if (!ConnectionManager) return;

	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;
	int32 HealthDiff = NewHealth - OldHealth;

	if (HealthDiff == 0) return;

	FLinearColor MessageColor;
	FString Message;

	if (Entity.Guid == LocalGuid)
	{
		// Player health change
		if (HealthDiff > 0)
		{
			MessageColor = FLinearColor::Green; // Healing
			Message = FString::Printf(TEXT("You are healed for %d"), HealthDiff);
		}
		else
		{
			MessageColor = FLinearColor::Red; // Damage
			Message = FString::Printf(TEXT("You take %d damage"), -HealthDiff);
		}
	}
	else
	{
		// Other entity health change
		MessageColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f); // Gray
		FString EntityName = FString::Printf(TEXT("Entity %llu"), Entity.Guid);

		if (HealthDiff > 0)
		{
			Message = FString::Printf(TEXT("%s is healed for %d"), *EntityName, HealthDiff);
		}
		else
		{
			Message = FString::Printf(TEXT("%s takes %d damage"), *EntityName, -HealthDiff);
		}
	}

	AddCombatMessage(Message, MessageColor);
}

void AWowGameplayController::CreateChatWindow()
{
	if (!ConnectionManager || ChatWindow.IsValid())
	{
		return;
	}

	// Create the chat window widget
	ChatWindow = SNew(SWowChatWindow)
		.ConnectionManager(ConnectionManager);

	// Bind input mode change delegate
	ChatWindow->OnInputModeChanged.BindUObject(this, &AWowGameplayController::OnChatInputModeChanged);
}

void AWowGameplayController::OnEnterKey()
{
	if (ChatWindow.IsValid())
	{
		ChatWindow->ToggleInputFocus();
	}
}

void AWowGameplayController::OnChatInputModeChanged(bool bGameAndUI)
{
	if (bGameAndUI)
	{
		SetInputMode(FInputModeGameAndUI());
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void AWowGameplayController::OnBagKey()
{
	if (UIManager)
	{
		UIManager->ToggleBagWindow();
	}
}

void AWowGameplayController::OnCharacterKey()
{
	if (UIManager)
	{
		UIManager->ToggleCharacterPanel();
	}
}

void AWowGameplayController::OnPlayerInventoryUpdated()
{
	if (UIManager)
	{
		UIManager->UpdateInventory();
	}
}
