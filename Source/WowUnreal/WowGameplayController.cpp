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
#include "WowAudioManager.h"
#include "WowCharacterBuilder.h"
#include "WowAnimationController.h"
#include "WowNameplateWidget.h"
#include "SWowCombatLog.h"
#include "SWowChatWindow.h"
#include "SWowPartyFrame.h"
#include "SWowPartyInvite.h"
#include "SWowTaxiMap.h"
#include "UI/SWowQuestLog.h"
#include "UI/SWowGuildRoster.h"
#include "UI/SWowMailbox.h"
#include "UI/SWowTalentWindow.h"
#include "WowDeathManager.h"
#include "WowCursorManager.h"
#include "WowSpellMissile.h"
#include "Formats/Dbc/DbcStore.h"
#include "WowTooltipManager.h"
#include "UI/SWowActionBar.h"
#include "UI/SWowMinimap.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "Coord/WowCoordinate.h"
#include "Components/WidgetComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Components/DecalComponent.h"
#include "Widgets/SViewport.h"
#include "SWowCastBar.h"
#include "WowFloatingText.h"
#include "Formats/Dbc/DbcStore.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWeakWidget.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/World.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowGameplay, Log, All);

namespace
{
TSharedPtr<SWidget> GetWowGameViewportWidget()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}

	return StaticCastSharedPtr<SWidget>(GEngine->GameViewport->GetGameViewportWidget());
}

void ApplyWowGameAndUiInputMode(APlayerController* PlayerController, const TSharedPtr<SWidget>& WidgetToFocus)
{
	if (!PlayerController)
	{
		return;
	}

	FInputModeGameAndUI InputMode;
	if (WidgetToFocus.IsValid())
	{
		InputMode.SetWidgetToFocus(WidgetToFocus);
	}
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PlayerController->SetInputMode(InputMode);
}

void FocusWowGameViewport()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetAllUserFocusToGameViewport(EFocusCause::SetDirectly);
	}
}

bool IsWowUiConsumingKeyboardInput()
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	const TSharedPtr<SWidget> FocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	if (!FocusedWidget.IsValid())
	{
		return false;
	}

	const TSharedPtr<SWidget> GameViewportWidget = GetWowGameViewportWidget();
	return !GameViewportWidget.IsValid() || FocusedWidget != GameViewportWidget;
}

bool ShouldSuppressGameplayHotkey(const FKey& Key)
{
	return Key == EKeys::Tab
		|| Key == EKeys::Enter
		|| Key == EKeys::One
		|| Key == EKeys::Two
		|| Key == EKeys::Three
		|| Key == EKeys::Four
		|| Key == EKeys::Five
		|| Key == EKeys::Six
		|| Key == EKeys::Seven
		|| Key == EKeys::Eight
		|| Key == EKeys::Nine
		|| Key == EKeys::Zero
		|| Key == EKeys::Hyphen
		|| Key == EKeys::Equals
		|| Key == EKeys::M
		|| Key == EKeys::B
		|| Key == EKeys::C
		|| Key == EKeys::L
		|| Key == EKeys::J
		|| Key == EKeys::N
		|| Key == EKeys::P;
}
}

AWowGameplayController::AWowGameplayController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;
	PrimaryActorTick.bCanEverTick = true;

	// Create Game UI component
	GameUI = CreateDefaultSubobject<UWowGameUI>(TEXT("WowGameUI"));
}

void AWowGameplayController::BeginPlay()
{
	Super::BeginPlay();
	ApplyWowGameAndUiInputMode(this, GetWowGameViewportWidget());
	FocusWowGameViewport();
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = true;

	// Cache UIManager early so OnUpdate ticks even without networking
	if (UGameInstance* GI = GetGameInstance())
	{
		UIManager = GI->GetSubsystem<UWowUIManager>();
		if (UIManager)
		{
			// Don't clear the root canvas — LoginController sets it up for FrameXML rendering.
			// The Lua UI frames need the canvas to exist for widget creation.
		}
	}

	// Create managers
	DeathManager = NewObject<UWowDeathManager>(this);
	CursorManager = NewObject<UWowCursorManager>(this);
	TooltipManager = NewObject<UWowTooltipManager>(this);

	// Cast bar — kept as fallback for spell feedback
	TSharedRef<SWidget> CastBarContainer =
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(0, 0, 0, 80)
		[
			SAssignNew(CastBarWidget, SWowCastBar)
		];
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetForPlayer(GetLocalPlayer(), CastBarContainer, 1000);
	}
}

void AWowGameplayController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Left click for targeting and WoW UI hit testing
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AWowGameplayController::OnLeftClick);

	// Right click for auto-attack - use IE_Pressed for immediate responsiveness
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AWowGameplayController::OnRightClick);

	// Action bar keybinds - use IE_Pressed for immediate responsiveness like WoW
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AWowGameplayController::OnActionSlot1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AWowGameplayController::OnActionSlot2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AWowGameplayController::OnActionSlot3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AWowGameplayController::OnActionSlot4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AWowGameplayController::OnActionSlot5);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AWowGameplayController::OnActionSlot6);
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &AWowGameplayController::OnActionSlot7);
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &AWowGameplayController::OnActionSlot8);
	InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &AWowGameplayController::OnActionSlot9);
	InputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &AWowGameplayController::OnActionSlot0);
	InputComponent->BindKey(EKeys::Hyphen, IE_Pressed, this, &AWowGameplayController::OnActionSlotMinus);
	InputComponent->BindKey(EKeys::Equals, IE_Pressed, this, &AWowGameplayController::OnActionSlotEquals);

	// Tab targeting — select closest enemy
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AWowGameplayController::OnTabTarget);

	// Minimap toggle
	InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AWowGameplayController::OnToggleMap);

	// Chat input
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AWowGameplayController::OnEnterKey);

	// Inventory UI keys
	InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AWowGameplayController::OnBagKey);
	InputComponent->BindKey(EKeys::C, IE_Pressed, this, &AWowGameplayController::OnCharacterKey);
	InputComponent->BindKey(EKeys::L, IE_Pressed, this, &AWowGameplayController::OnQuestLogKey);
	InputComponent->BindKey(EKeys::J, IE_Pressed, this, &AWowGameplayController::OnGuildRosterKey);
	InputComponent->BindKey(EKeys::N, IE_Pressed, this, &AWowGameplayController::OnTalentKey);
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AWowGameplayController::OnSpellbookKey);
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
	ConnectionManager->PacketHandler.OnEntityDeath.AddUObject(
		this, &AWowGameplayController::OnEntityDeath);

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

	// Bind spell and action bar events
	ConnectionManager->PacketHandler.OnInitialSpells.AddUObject(
		this, &AWowGameplayController::OnInitialSpells);
	ConnectionManager->PacketHandler.OnActionButtonsUpdated.AddUObject(
		this, &AWowGameplayController::OnActionButtonsUpdated);

	// Bind emote events
	ConnectionManager->PacketHandler.OnEmote.AddUObject(
		this, &AWowGameplayController::OnEmote);

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

	// Listen for spell cooldowns to update action bar
	ConnectionManager->PacketHandler.OnSpellCooldown.AddLambda([this](uint32 SpellId, float Duration)
	{
		if (ActionBarWidget.IsValid())
		{
			ActionBarWidget->UpdateCooldown(SpellId, Duration);
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

	// Fire UI events for entering the world
	FireUIEvent(TEXT("PLAYER_ENTERING_WORLD"));
	FireUIEvent(TEXT("PLAYER_TARGET_CHANGED"));

	// Re-fire action bar events — action data may have arrived before UI was ready
	if (ConnectionManager)
	{
		const TArray<uint32>& AB = ConnectionManager->PacketHandler.ActionButtons;
		for (int32 i = 0; i < FMath::Min(AB.Num(), 120); ++i)
		{
			if (AB[i] != 0)
			{
				FireUIEvent(TEXT("ACTIONBAR_SLOT_CHANGED"), {FString::Printf(TEXT("%d"), i + 1)});
			}
		}
		FireUIEvent(TEXT("ACTIONBAR_UPDATE_STATE"));
	}
}

void AWowGameplayController::ApplyDeferredSpawn()
{
	if (!bHasDeferredSpawn) return;
	bHasDeferredSpawn = false;
	ApplyDeferredSpawn_Internal(DeferredSpawnPos, DeferredSpawnOrientation);

	// Fire UI events for entering the world
	FireUIEvent(TEXT("PLAYER_ENTERING_WORLD"));
	FireUIEvent(TEXT("PLAYER_TARGET_CHANGED"));

	// Re-fire action bar events — action data may have arrived before UI was ready
	if (ConnectionManager)
	{
		const TArray<uint32>& AB = ConnectionManager->PacketHandler.ActionButtons;
		for (int32 i = 0; i < FMath::Min(AB.Num(), 120); ++i)
		{
			if (AB[i] != 0)
			{
				FireUIEvent(TEXT("ACTIONBAR_SLOT_CHANGED"), {FString::Printf(TEXT("%d"), i + 1)});
			}
		}
		FireUIEvent(TEXT("ACTIONBAR_UPDATE_STATE"));
	}
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

	// Lua FrameXML is the primary UI — no Slate fallbacks.
}

void AWowGameplayController::OnEntityUpdated(const FWowEntity& Entity)
{
	if (!ConnectionManager) return;

	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;

	// Track health changes for combat log and death detection
	static TMap<uint64, int32> LastKnownHealth;
	static TMap<uint64, int32> LastKnownMana;
	int32 CurrentHealth = Entity.GetHealth();
	int32 CurrentMana = 0;
	if (Entity.IsUnit())
	{
		const FWowUnitEntity* UnitEntity = static_cast<const FWowUnitEntity*>(&Entity);
		CurrentMana = UnitEntity->GetPower(0); // 0 = mana
	}
	int32* OldHealthPtr = LastKnownHealth.Find(Entity.Guid);
	int32* OldManaPtr = LastKnownMana.Find(Entity.Guid);

	bool HealthChanged = (OldHealthPtr && *OldHealthPtr != CurrentHealth) || (!OldHealthPtr && CurrentHealth > 0);
	bool ManaChanged = Entity.IsUnit() && ((OldManaPtr && *OldManaPtr != CurrentMana) || (!OldManaPtr && CurrentMana > 0));

	if (HealthChanged && OldHealthPtr)
	{
		OnEntityHealthChanged(Entity, *OldHealthPtr, CurrentHealth);
	}

	// Fire UI events for health/mana changes
	if (Entity.Guid == LocalGuid)
	{
		if (HealthChanged)
		{
			FireUIEvent(TEXT("UNIT_HEALTH"), {TEXT("player")});
		}
		if (ManaChanged)
		{
			FireUIEvent(TEXT("UNIT_MANA"), {TEXT("player")});
		}

		// Check for combat state changes on local player
		if (Entity.IsUnit())
		{
			const FWowUnitEntity* UnitEntity = static_cast<const FWowUnitEntity*>(&Entity);
			uint32 UnitFlags = UnitEntity->GetUnitFlags();
			bool bIsInCombat = (UnitFlags & 0x00080000) != 0; // UNIT_FLAG_IN_COMBAT

			if (bIsInCombat != bWasInCombat)
			{
				if (bIsInCombat)
				{
					FireUIEvent(TEXT("PLAYER_ENTER_COMBAT"));
					FireUIEvent(TEXT("PLAYER_REGEN_DISABLED"));
				}
				else
				{
					FireUIEvent(TEXT("PLAYER_LEAVE_COMBAT"));
					FireUIEvent(TEXT("PLAYER_REGEN_ENABLED"));
				}
				bWasInCombat = bIsInCombat;
			}

			// Check for player flags changes
			static uint32 LastPlayerFlags = 0;
			if (LastPlayerFlags != UnitFlags && LastPlayerFlags != 0)
			{
				FireUIEvent(TEXT("PLAYER_FLAGS_CHANGED"));
			}
			LastPlayerFlags = UnitFlags;
		}
	}
	else if (Entity.Guid == TargetGuid)
	{
		if (HealthChanged)
		{
			FireUIEvent(TEXT("UNIT_HEALTH"), {TEXT("target")});
		}
		if (ManaChanged)
		{
			FireUIEvent(TEXT("UNIT_MANA"), {TEXT("target")});
		}
	}

	// Only start tracking once we've seen a positive health value
	// This prevents false death triggers from entities whose fields aren't populated yet
	if (CurrentHealth > 0 || OldHealthPtr)
	{
		LastKnownHealth.Add(Entity.Guid, CurrentHealth);
	}
	if (Entity.IsUnit() && (CurrentMana > 0 || OldManaPtr))
	{
		LastKnownMana.Add(Entity.Guid, CurrentMana);
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

		// Server position tracking (correction disabled — causes rubber-banding)
		// The client is authoritative for movement; server corrections only
		// needed for teleports which are handled via OnTeleportRequest
		if (Entity.Movement.Position != FVector::ZeroVector)
		{
			LastServerPosition = FWowCoordinate::WowToUE(Entity.Movement.Position);
		}
	}
	else
	{
		// Non-local entity: update actor position if we spawned one
		// Don't override position when entity is moving via spline (SMSG_MONSTER_MOVE).
		// Instead, smoothly lerp to the server position to avoid teleporting.
		TObjectPtr<AActor>* ActorPtr = SpawnedEntityActors.Find(Entity.Guid);
		if (ActorPtr && *ActorPtr && Entity.Movement.Position != FVector::ZeroVector
			&& !Entity.Movement.bHasActiveSpline && !DeadEntityGuids.Contains(Entity.Guid))
		{
			FVector UEPos = FWowCoordinate::WowToUE(Entity.Movement.Position);
			if (!UEPos.ContainsNaN())
			{
				// Smooth interpolation instead of teleporting
				FVector CurrentPos = (*ActorPtr)->GetActorLocation();
				float Dist = FVector::Dist(CurrentPos, UEPos);
				if (Dist > 5000.0f) // > 50 yards = teleport (too far to lerp)
				{
					(*ActorPtr)->SetActorLocation(UEPos);
				}
				else if (Dist > 5.0f) // Small threshold to avoid jitter
				{
					// Lerp toward server position (smooth over ~0.3s at 60fps)
					FVector Smoothed = FMath::VInterpTo(CurrentPos, UEPos, GetWorld()->GetDeltaSeconds(), 5.0f);
					(*ActorPtr)->SetActorLocation(Smoothed);
				}

				float Ori = FMath::IsFinite(Entity.Movement.Orientation) ? Entity.Movement.Orientation : 0.0f;
				float UEYaw = -90.0f - FMath::RadiansToDegrees(Ori);
				(*ActorPtr)->SetActorRotation(FRotator(0.0f, UEYaw, 0.0f));
			}
		}

		// Check for death — entity health=0 with valid MaxHealth, or StandState=DEAD
		if (!DeadEntityGuids.Contains(Entity.Guid) && Entity.Guid != LocalGuid)
		{
			int32 EntityHealth = Entity.GetHealth();
			int32 EntityMaxHealth = Entity.GetMaxHealth();
			uint8 StandState = Entity.GetFieldByte(UnitField::BYTES_1, 0);

			if ((EntityHealth <= 0 && EntityMaxHealth > 0) || StandState == WowStandState::DEAD)
			{
				UE_LOG(LogWowGameplay, Log, TEXT("OnEntityUpdated: Entity %llu is dead (health=%d, maxHealth=%d, standState=%d)"),
					Entity.Guid, EntityHealth, EntityMaxHealth, StandState);
				OnEntityDeath(Entity.Guid);
			}
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

	// Update XP bar (throttled to avoid excessive updates)
	static float XPUpdateTimer = 0.0f;
	XPUpdateTimer += DeltaTime;
	if (XPUpdateTimer >= 0.5f) // Update every 500ms
	{
		XPUpdateTimer = 0.0f;
		UpdateXPBar();
	}

	// Update quest tracker (throttled to avoid excessive updates)
	static float QuestUpdateTimer = 0.0f;
	QuestUpdateTimer += DeltaTime;
	if (QuestUpdateTimer >= 2.0f) // Update every 2 seconds
	{
		QuestUpdateTimer = 0.0f;
		UpdateQuestTracker();
	}

	// Update target frame display
	UpdateTargetFrame();

	// Update aura display (throttled)
	AuraUpdateTimer += DeltaTime;
	if (AuraUpdateTimer >= AuraUpdateInterval)
	{
		AuraUpdateTimer = 0.0f;
		UpdateAuraDisplay();
	}
}

void AWowGameplayController::SendMovementUpdate()
{
	ACharacter* Char = Cast<ACharacter>(GetPawn());
	if (!Char || !bHasServerPosition) return;

	FVector Pos = Char->GetActorLocation();
	FVector WowPos = FWowCoordinate::UEToWow(Pos);

	// Convert UE yaw to WoW orientation
	// UE yaw and WoW orientation relationship: WoW_ori = -(UE_yaw_rad)
	// (from the coordinate transform: WoW +X = UE +X, WoW +Y = UE -Y)
	float UEYawRad = FMath::DegreesToRadians(Char->GetActorRotation().Yaw);
	float WowOrientation = -UEYawRad;

	// Derive MoveFlags from character movement state
	uint32 MoveFlags = 0;
	UCharacterMovementComponent* Movement = Char->GetCharacterMovement();
	if (Movement)
	{
		FVector Velocity = Movement->Velocity;
		float Speed2D = Velocity.Size2D();

		if (Speed2D > 10.0f)
		{
			// Determine forward/backward based on velocity vs facing direction
			FVector Forward = Char->GetActorForwardVector();
			float Dot = FVector::DotProduct(Forward, Velocity.GetSafeNormal2D());
			if (Dot > 0.3f)
				MoveFlags |= 0x00000001; // FORWARD
			else if (Dot < -0.3f)
				MoveFlags |= 0x00000002; // BACKWARD

			// Check strafing
			FVector Right = Char->GetActorRightVector();
			float SideDot = FVector::DotProduct(Right, Velocity.GetSafeNormal2D());
			if (SideDot > 0.5f)
				MoveFlags |= 0x00000008; // STRAFE_RIGHT
			else if (SideDot < -0.5f)
				MoveFlags |= 0x00000004; // STRAFE_LEFT
		}

		if (Movement->IsSwimming())
			MoveFlags |= 0x00200000; // SWIMMING

		if (Movement->IsFalling())
		{
			if (Velocity.Z > 0)
				MoveFlags |= 0x00002000; // JUMPING
			else
				MoveFlags |= 0x00001000; // FALLING
		}

		if (Movement->IsWalking() && Speed2D > 10.0f)
			MoveFlags |= 0x00000100; // WALK_MODE
	}

	// Always send heartbeat (even when stationary — server needs to know we're alive and where)
	bool bPositionChanged = !Pos.Equals(LastSentPosition, 1.0f);
	bool bFlagsChanged = (MoveFlags != 0);

	if (bPositionChanged || bFlagsChanged)
	{
		LastSentPosition = Pos;
		ConnectionManager->SendMovement(WowOpcode::MSG_MOVE_HEARTBEAT, WowPos, WowOrientation, MoveFlags);
	}
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

bool AWowGameplayController::InputKey(const FInputKeyEventArgs& Params)
{
	if (Params.Event == IE_Pressed
		&& IsWowUiConsumingKeyboardInput()
		&& ShouldSuppressGameplayHotkey(Params.Key))
	{
		return true;
	}

	return Super::InputKey(Params);
}

void AWowGameplayController::OnLeftClick()
{
	// Check if a WoW UI frame is under the cursor first
	if (UIManager && UIManager->GetFrameManager())
	{
		float MX, MY;
		if (GetMousePosition(MX, MY))
		{
			int64 HitFrame = UIManager->GetFrameManager()->HitTestFrames(MX, MY);
			if (HitFrame >= 0)
			{
				if (ConnectionManager && ConnectionManager->HasCursorPayload()
					&& UIManager->GetFrameManager()->DispatchReceiveDrag(HitFrame))
				{
					return; // Consume drag-drop on WoW UI before regular click handling
				}

				UIManager->GetFrameManager()->DispatchMouseDown(HitFrame, TEXT("LeftButton"));
				UIManager->GetFrameManager()->DispatchClick(HitFrame, TEXT("LeftButton"));
				UIManager->GetFrameManager()->DispatchMouseUp(HitFrame, TEXT("LeftButton"));
				return; // Don't target through UI
			}
		}
	}

	UE_LOG(LogWowGameplay, Log, TEXT("OnLeftClick fired"));
	TryTargetUnderCursor();
}

void AWowGameplayController::TryTargetUnderCursor()
{
	if (!ConnectionManager) return;

	// Get mouse position and deproject to world
	float MouseX, MouseY;
	if (!GetMousePosition(MouseX, MouseY))
	{
		UE_LOG(LogWowGameplay, Verbose, TEXT("TryTarget: no mouse position"));
		return;
	}

	FVector WorldLoc, WorldDir;
	if (!DeprojectScreenPositionToWorld(MouseX, MouseY, WorldLoc, WorldDir))
	{
		UE_LOG(LogWowGameplay, Verbose, TEXT("TryTarget: deproject failed"));
		return;
	}

	// Line trace 10000 cm (~100 WoW yards)
	FVector TraceEnd = WorldLoc + WorldDir * 1000000.0f;
	FHitResult Hit;
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(GetPawn());
	TraceParams.bTraceComplex = false; // Use simple collision (faster, more reliable)

	UWorld* World = GetWorld();
	if (!World) return;

	// Try ECC_Pawn first (entity meshes), then ECC_WorldStatic for terrain
	bool bHit = World->LineTraceSingleByChannel(Hit, WorldLoc, TraceEnd, ECC_Pawn, TraceParams);
	if (!bHit)
	{
		bHit = World->LineTraceSingleByChannel(Hit, WorldLoc, TraceEnd, ECC_Visibility, TraceParams);
	}

	if (bHit && Hit.GetActor())
	{
		AActor* HitActor = Hit.GetActor();

		// Find GUID from spawned entity actors map (reverse lookup — more reliable than tag parsing)
		for (const auto& Pair : SpawnedEntityActors)
		{
			if (Pair.Value == HitActor)
			{
				uint64 HitGuid = Pair.Key;
				if (HitGuid != TargetGuid)
				{
					SetTarget(HitGuid);
				}
				return;
			}
		}

		// Also check if we hit a child component (capsule) of an entity actor
		AActor* Owner = HitActor->GetOwner();
		if (Owner)
		{
			for (const auto& Pair : SpawnedEntityActors)
			{
				if (Pair.Value == Owner)
				{
					uint64 HitGuid = Pair.Key;
					if (HitGuid != TargetGuid)
					{
						SetTarget(HitGuid);
					}
					return;
				}
			}
		}

		UE_LOG(LogWowGameplay, Verbose, TEXT("Hit non-entity: %s"), *HitActor->GetName());
	}

	// Clicked on nothing or non-entity — clear target
	if (TargetGuid != 0)
	{
		SetTarget(0);
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

			// Initialize audio system
			InitializeAudioSystem(WM);
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

void AWowGameplayController::OnEntityDeath(uint64 Guid)
{
	UE_LOG(LogWowGameplay, Log, TEXT("Entity %llu died"), Guid);

	// Prevent double-death
	if (DeadEntityGuids.Contains(Guid)) return;

	// Track as dead/lootable
	DeadEntityGuids.Add(Guid);

	// Stop any active spline movement — dead entities don't walk
	if (ConnectionManager)
	{
		FWowEntity* Entity = ConnectionManager->PacketHandler.EntityManager.Find(Guid);
		if (Entity)
		{
			Entity->Movement.bHasActiveSpline = false;
			Entity->Movement.SplineWaypoints.Empty();
			Entity->Movement.MoveFlags = 0;
		}
	}

	// Play death animation on the actor
	TObjectPtr<AActor>* ActorPtr = SpawnedEntityActors.Find(Guid);
	if (ActorPtr && *ActorPtr)
	{
		UWowAnimationController* AnimController = FWowCharacterBuilder::GetAnimationController(*ActorPtr);
		if (AnimController)
		{
			AnimController->PlayAnimationById(EWowAnimId::Death, false);
			UE_LOG(LogWowGameplay, Log, TEXT("Playing death animation for entity %llu"), Guid);
		}
	}

	// Stop auto-attack if we were attacking this entity
	if (bIsAutoAttacking && AutoAttackTargetGuid == Guid)
	{
		StopAutoAttack();
	}

	// Remove from entity manager after a delay (keep corpse visible for looting)
	// The entity actor stays in SpawnedEntityActors so it can be right-clicked for loot.
	// We remove it from the entity manager so the server can reuse the GUID.
	if (ConnectionManager)
	{
		ConnectionManager->PacketHandler.EntityManager.Remove(Guid);
	}

	// Auto-remove corpse after 60 seconds
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, [this, Guid]()
	{
		DeadEntityGuids.Remove(Guid);
		TObjectPtr<AActor>* CorpsePtr = SpawnedEntityActors.Find(Guid);
		if (CorpsePtr && *CorpsePtr)
		{
			(*CorpsePtr)->Destroy();
		}
		SpawnedEntityActors.Remove(Guid);
		EntityNameplates.Remove(Guid);
		UE_LOG(LogWowGameplay, Log, TEXT("Removed corpse for entity %llu"), Guid);
	}, 60.0f, false);
}

void AWowGameplayController::OnEntityDestroyed(uint64 Guid)
{
	// If this is a dead corpse, keep it visible for looting — don't destroy yet
	if (DeadEntityGuids.Contains(Guid))
	{
		UE_LOG(LogWowGameplay, Log, TEXT("Entity %llu destroyed by server but keeping corpse for looting"), Guid);
		return;
	}

	TObjectPtr<AActor>* ActorPtr = SpawnedEntityActors.Find(Guid);
	if (ActorPtr && *ActorPtr)
	{
		(*ActorPtr)->Destroy();
		UE_LOG(LogWowGameplay, Log, TEXT("Destroyed entity model for GUID %llu"), Guid);
	}
	SpawnedEntityActors.Remove(Guid);

	// Clean up nameplate
	EntityNameplates.Remove(Guid);

	// Clean up target highlight if this was our target
	if (TargetGuid == Guid)
	{
		if (TargetHighlightCircle)
		{
			TargetHighlightCircle->DestroyComponent();
			TargetHighlightCircle = nullptr;
		}
	}
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
	// Guard against NaN from bad server data
	if (UEPos.ContainsNaN()) UEPos = FVector::ZeroVector;

	float OriDeg = FMath::IsFinite(Entity.Movement.Orientation) ? Entity.Movement.Orientation : 0.0f;
	FRotator Rot(0.0f, -90.0f - FMath::RadiansToDegrees(OriDeg), 0.0f);

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

		// Add a capsule collision for click targeting (skeletal meshes have no physics asset)
		UCapsuleComponent* TargetCapsule = NewObject<UCapsuleComponent>(SpawnedActor, TEXT("TargetCapsule"));
		if (TargetCapsule)
		{
			TargetCapsule->SetupAttachment(SpawnedActor->GetRootComponent());
			TargetCapsule->SetCapsuleSize(50.0f, 100.0f); // Roughly character-sized
			TargetCapsule->SetRelativeLocation(FVector(0, 0, 100.0f)); // Center at waist height
			TargetCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			TargetCapsule->SetCollisionObjectType(ECC_Pawn);
			TargetCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
			TargetCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			TargetCapsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			TargetCapsule->ShapeColor = FColor::Green;
			TargetCapsule->SetHiddenInGame(true);
			TargetCapsule->RegisterComponent();
		}

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

		// Skip dead entities — corpses don't move
		if (DeadEntityGuids.Contains(Entity.Guid)) continue;

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
			if (UEPos.ContainsNaN()) continue; // Skip invalid positions
			(*ActorPtr)->SetActorLocation(UEPos);
			float Ori = FMath::IsFinite(Entity.Movement.Orientation) ? Entity.Movement.Orientation : 0.0f;
			float UEYaw = -90.0f - FMath::RadiansToDegrees(Ori);
			(*ActorPtr)->SetActorRotation(FRotator(0.0f, UEYaw, 0.0f));
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

		// Get target name for combat message
		FString TargetName = TEXT("Unknown");
		const FWowEntity* TargetEntity = ConnectionManager->PacketHandler.EntityManager.Find(TargetGuid);
		if (TargetEntity)
		{
			if (TargetEntity->IsPlayer())
			{
				TargetName = FString::Printf(TEXT("Player %llu"), TargetGuid);
			}
			else if (TargetEntity->IsUnit())
			{
				TargetName = FString::Printf(TEXT("Creature %u"), TargetEntity->Entry);

				const FDbcStore& DbcStore = FDbcStore::Get();
				if (DbcStore.IsLoaded())
				{
					// Try to get creature name from DBC if available
					TargetName = FString::Printf(TEXT("Creature %u"), TargetEntity->Entry);
				}
			}
		}

		// Add combat message
		FString Message = FString::Printf(TEXT("You begin attacking %s"), *TargetName);
		AddCombatMessage(Message, FLinearColor::Red);
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
	// Check if a WoW UI frame is under the cursor first
	if (UIManager && UIManager->GetFrameManager())
	{
		float MX, MY;
		if (GetMousePosition(MX, MY))
		{
			int64 HitFrame = UIManager->GetFrameManager()->HitTestFrames(MX, MY);
			if (HitFrame >= 0)
			{
				UIManager->GetFrameManager()->DispatchMouseDown(HitFrame, TEXT("RightButton"));
				UIManager->GetFrameManager()->DispatchClick(HitFrame, TEXT("RightButton"));
				UIManager->GetFrameManager()->DispatchMouseUp(HitFrame, TEXT("RightButton"));
				return; // Don't interact through UI
			}
		}
	}

	UE_LOG(LogWowGameplay, Log, TEXT("OnRightClick: TargetGuid=%llu DeadEntities=%d"), TargetGuid, DeadEntityGuids.Num());

	if (TargetGuid == 0)
	{
		// No target - try to target something under cursor first
		TryTargetUnderCursor();
		UE_LOG(LogWowGameplay, Log, TEXT("OnRightClick: after TryTarget, TargetGuid=%llu"), TargetGuid);
		if (TargetGuid == 0) return;
	}

	// Check if this is a dead NPC corpse — loot it
	UE_LOG(LogWowGameplay, Log, TEXT("OnRightClick: checking DeadEntityGuids for %llu, contains=%d"),
		TargetGuid, DeadEntityGuids.Contains(TargetGuid) ? 1 : 0);
	if (DeadEntityGuids.Contains(TargetGuid))
	{
		UE_LOG(LogWowGameplay, Log, TEXT("Right-click: looting dead entity %llu"), TargetGuid);
		if (ConnectionManager)
		{
			ConnectionManager->SendLoot(static_cast<int64>(TargetGuid));
		}
		return;
	}

	const FWowEntity* TargetEntity = nullptr;
	if (ConnectionManager)
	{
		TargetEntity = ConnectionManager->PacketHandler.EntityManager.Find(TargetGuid);
	}

	if (!TargetEntity)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Right-click: target %llu not found in entity manager"), TargetGuid);
		return;
	}

	UE_LOG(LogWowGameplay, Log, TEXT("Right-click: target=%llu IsUnit=%d IsPlayer=%d TypeMask=0x%08X Kind=%d"),
		TargetGuid, TargetEntity->IsUnit() ? 1 : 0, TargetEntity->IsPlayer() ? 1 : 0,
		TargetEntity->TypeMask, static_cast<int32>(TargetEntity->GetEntityKind()));

	if (!TargetEntity->IsUnit()) return;

	// Players: do nothing (no PvP interaction yet)
	if (TargetEntity->IsPlayer()) return;

	// Check if entity is dead (health = 0) — loot instead of interact
	int32 Health = TargetEntity->GetHealth();
	if (Health <= 0)
	{
		UE_LOG(LogWowGameplay, Log, TEXT("Right-click: entity %llu has 0 health, looting"), TargetGuid);
		ConnectionManager->SendLoot(static_cast<int64>(TargetGuid));
		return;
	}

	// Check NPC flags to determine interaction type
	uint32 NpcFlags = TargetEntity->GetField(UnitField::NPC_FLAGS);
	UE_LOG(LogWowGameplay, Log, TEXT("Right-click NPC %llu: NpcFlags=0x%08X"), TargetGuid, NpcFlags);

	if (NpcFlags & WowNpcFlags::INTERACTABLE)
	{
		// This NPC has interactable flags — send gossip/questgiver hello
		UE_LOG(LogWowGameplay, Log, TEXT("  -> Sending gossip hello (interactable NPC)"));
		ConnectionManager->SendGossipHello(static_cast<int64>(TargetGuid));
	}
	else
	{
		// No interactable flags — treat as hostile, start auto-attack
		UE_LOG(LogWowGameplay, Log, TEXT("  -> Starting auto-attack (hostile NPC)"));
		StartAutoAttack();
	}
}

void AWowGameplayController::OnTabTarget()
{
	if (!ConnectionManager) return;

	APawn* MyPawn = GetPawn();
	if (!MyPawn) return;

	FVector MyPos = MyPawn->GetActorLocation();
	float ClosestDist = MAX_FLT;
	uint64 ClosestGuid = 0;
	AActor* ClosestActor = nullptr;

	// Find closest NPC/hostile entity (skip current target to cycle)
	for (const auto& Pair : SpawnedEntityActors)
	{
		if (!Pair.Value || !IsValid(Pair.Value)) continue;
		if (Pair.Key == TargetGuid) continue; // Skip current target to cycle

		// Only target units (not players for tab target)
		const FWowEntity* Entity = ConnectionManager->PacketHandler.EntityManager.Find(Pair.Key);
		if (!Entity || !Entity->IsUnit() || Entity->IsPlayer()) continue;

		float Dist = FVector::Dist(MyPos, Pair.Value->GetActorLocation());
		if (Dist < ClosestDist && Dist < 4000000.0f) // ~400 yards max range
		{
			ClosestDist = Dist;
			ClosestGuid = Pair.Key;
			ClosestActor = Pair.Value;
		}
	}

	if (ClosestGuid != 0)
	{
		SetTarget(ClosestGuid);
	}
	else if (TargetGuid != 0)
	{
		// No other targets — clear
		SetTarget(0);
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

		// Play casting animation for local player if cast time > 0
		if (CastTime > 0)
		{
			if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn()))
			{
				if (UWowAnimationController* AnimController = PlayerChar->AnimationController)
				{
					AnimController->PlayAnimationById(EWowAnimId::SpellCastDirected, true);
					UE_LOG(LogWowGameplay, Log, TEXT("Playing spell cast animation for local player"));
				}
			}
		}

		// Also log to combat log
		FString Message = FString::Printf(TEXT("You cast %s"), *SpellName);
		AddCombatMessage(Message, FLinearColor::Yellow);

		// Fire spell casting events
		FireUIEvent(TEXT("UNIT_SPELLCAST_START"), {TEXT("player")});
		FireUIEvent(TEXT("CURRENT_SPELL_CAST_CHANGED"));

		// Spawn hand glow effects for local player
		SpawnCastGlowEffects(CasterGuid, SpellId);
	}

	// Spawn spell visual effects for all casters
	uint64 SpellTargetGuid = 0; // Will be resolved in SpawnSpellVisualEffect
	SpawnSpellVisualEffect(CasterGuid, SpellTargetGuid, SpellId, false);

	if (CasterGuid != LocalGuid)
	{
		// Other entity casting - play their cast animation if cast time > 0
		if (CastTime > 0)
		{
			if (TObjectPtr<AActor>* FoundActor = SpawnedEntityActors.Find(CasterGuid))
			{
				if (AActor* CasterActor = FoundActor->Get())
				{
					if (UWowAnimationController* AnimController = FWowCharacterBuilder::GetAnimationController(CasterActor))
					{
						AnimController->PlayAnimationById(EWowAnimId::SpellCastDirected, true);
						UE_LOG(LogWowGameplay, Log, TEXT("Playing spell cast animation for entity %llu"), CasterGuid);
					}
				}
			}
		}

		// Other entity casting
		UE_LOG(LogWowGameplay, Log, TEXT("Entity %llu started casting spell %u"), CasterGuid, SpellId);

		// Log to combat log
		FString CasterName = FString::Printf(TEXT("Entity %llu"), CasterGuid);
		FString Message = FString::Printf(TEXT("%s cast %s"), *CasterName, *SpellName);
		AddCombatMessage(Message, FLinearColor(0.8f, 0.8f, 0.8f, 1.0f));

		// Spawn hand glow effects for other entities
		SpawnCastGlowEffects(CasterGuid, SpellId);
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

		// Return to idle animation for local player
		if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn()))
		{
			if (UWowAnimationController* AnimController = PlayerChar->AnimationController)
			{
				AnimController->PlayAnimationById(EWowAnimId::Stand, true);
				UE_LOG(LogWowGameplay, Log, TEXT("Returning local player to idle animation"));
			}
		}

		// Remove hand glow effects
		RemoveCastGlowEffects();

		// Fire spell completion events
		FireUIEvent(TEXT("UNIT_SPELLCAST_SUCCEEDED"), {TEXT("player")});
		FireUIEvent(TEXT("SPELL_UPDATE_COOLDOWN"));
		FireUIEvent(TEXT("ACTIONBAR_UPDATE_COOLDOWN"));
	}
	else
	{
		// Return other entity to idle animation
		if (TObjectPtr<AActor>* FoundActor = SpawnedEntityActors.Find(CasterGuid))
		{
			if (AActor* CasterActor = FoundActor->Get())
			{
				if (UWowAnimationController* AnimController = FWowCharacterBuilder::GetAnimationController(CasterActor))
				{
					AnimController->PlayAnimationById(EWowAnimId::Stand, true);
					UE_LOG(LogWowGameplay, Log, TEXT("Returning entity %llu to idle animation"), CasterGuid);
				}
			}
		}

		UE_LOG(LogWowGameplay, Log, TEXT("Entity %llu completed spell %u"), CasterGuid, SpellId);

		// Remove hand glow effects for other entities - they use the same global array
		// This is okay because only one entity can be casting at a time in our system
		RemoveCastGlowEffects();
	}

	// Check if spell should spawn a missile
	uint64 SpellTargetGuid = 0; // Will be resolved in SpawnSpellMissile
	SpawnSpellMissile(CasterGuid, SpellTargetGuid, SpellId);
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

	// Pass connection manager to UI manager for character stats
	if (UIManager)
	{
		UIManager->SetConnectionManager(ConnectionManager);
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

	// Always look up cached character info (for equipment data + customization fallback)
	const FWowCharacterInfo* CachedCharInfo = nullptr;
	if (ConnectionManager)
	{
		const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;
		const TArray<FWowCharacterInfo>& CachedChars = ConnectionManager->GetCachedCharacters();

		for (const FWowCharacterInfo& CharInfo : CachedChars)
		{
			if (static_cast<uint64>(CharInfo.Guid) == LocalGuid)
			{
				CachedCharInfo = &CharInfo;
				if (RaceId == 0)
				{
					RaceId = CharInfo.Race;
					Gender = CharInfo.Gender;
				}
				UE_LOG(LogWowGameplay, Log, TEXT("Found cached character: Race=%d Gender=%d Equipment=%d slots"),
					CharInfo.Race, CharInfo.Gender, CharInfo.Equipment.Num());
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


	// Use equipment-aware character model if we have equipment data
	if (CachedCharInfo && CachedCharInfo->Equipment.Num() > 0)
	{
		PlayerChar->SetCharacterModelWithEquipment(World, CachedMpq, CachedAssetCache,
			RaceId, Gender, SkinColor, Face, HairStyle, HairColor, FacialHair, &CachedCharInfo->Equipment);
	}
	else
	{
		// Fallback to simple character model without equipment
		PlayerChar->SetCharacterModel(World, CachedMpq, CachedAssetCache,
			RaceId, Gender, SkinColor, Face, HairStyle, HairColor, FacialHair);
	}
}

void AWowGameplayController::UpdatePlayerAnimations()
{
	// Update local player animation based on character movement
	if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn()))
	{
		// If the player has an animation controller, update it
		if (UWowAnimationController* AnimController = PlayerChar->AnimationController)
		{
			// Don't override casting animations with movement animations
			// The casting animation will be explicitly set/unset in spell handlers
			if (!bIsCasting)
			{
				AnimController->UpdateLocalPlayerState(PlayerChar);
			}
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

		// Skip dead entities (corpses) — keep their death animation
		if (DeadEntityGuids.Contains(Guid)) continue;

		// Get the animation controller for this actor
		UWowAnimationController* AnimController = FWowCharacterBuilder::GetAnimationController(Actor);
		if (!AnimController) continue;

		// Get the entity data to determine animation state
		const FWowEntity* Entity = ConnectionManager->PacketHandler.EntityManager.Find(Guid);
		if (!Entity) continue;

		// Check server-authoritative stand state from UNIT_FIELD_BYTES_1
		uint8 StandState = Entity->GetFieldByte(UnitField::BYTES_1, 0);
		if (StandState == WowStandState::DEAD)
		{
			// Server says this entity is dead — trigger death handling
			if (!DeadEntityGuids.Contains(Guid))
			{
				OnEntityDeath(Guid);
			}
			continue;
		}

		// If already marked dead by OnEntityHealthChanged, skip
		// Note: Do NOT check GetHealth()==0 here — fields may not be populated yet
		// for newly spawned entities. Death is detected in OnEntityHealthChanged
		// when health transitions from >0 to 0, or by StandState==DEAD above.
		if (Entity->GetHealth() <= 0 && Entity->GetMaxHealth() > 0 && DeadEntityGuids.Contains(Guid))
		{
			continue;
		}

		// Determine NPC movement state for animation.
		// For NPCs, we rely on spline state (from SMSG_MONSTER_MOVE) rather than
		// raw MoveFlags from SMSG_UPDATE_OBJECT which can be stale.
		FWowMovementInfo AnimMovement = Entity->Movement;
		bool bIsOnSpline = Entity->Movement.bHasActiveSpline;

		// Clear server movement flags for NPCs — we determine movement from spline only
		if (!Entity->IsPlayer())
		{
			AnimMovement.MoveFlags = 0;
		}

		if (bIsOnSpline)
		{
			AnimMovement.MoveFlags |= WowMovementFlags::FORWARD;
		}

		// Don't override explicit cast/attack animations with movement updates
		EWowAnimState CurrentAnimState = AnimController->GetCurrentState();
		if (CurrentAnimState == EWowAnimState::Casting || CurrentAnimState == EWowAnimState::Channeling)
		{
			continue;
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

void AWowGameplayController::CreateXPBar()
{
	if (!ConnectionManager || XPBar.IsValid())
	{
		return;
	}

	// Create XP bar with overlay for text
	TSharedRef<SWidget> XPBarContainer =
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible) // Pass clicks through to 3D world
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)  // Full width
		.VAlign(VAlign_Bottom)
		.Padding(0, 0, 0, 70)  // Position above action bar (70px from bottom)
		[
			SNew(SBox)
			.HeightOverride(10) // Fixed height of 10px
			[
				// XP progress bar
				SAssignNew(XPBar, SProgressBar)
				.Style(&FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>("ProgressBar"))
				.BarFillType(EProgressBarFillType::LeftToRight)
				.FillColorAndOpacity(FLinearColor(0.5f, 0.0f, 1.0f, 0.8f)) // Purple fill like WoW
				.BackgroundImage(FCoreStyle::Get().GetBrush("ProgressBar.Background"))
				.Percent(0.0f)
			]
		]
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(0, 0, 0, 70)
		[
			// XP text overlay
			SAssignNew(XPText, STextBlock)
			.Text(FText::FromString(TEXT("XP: 0 / 0 (0%)")))
			.ColorAndOpacity(FLinearColor::White)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			.Justification(ETextJustify::Center)
		];

	// Add to viewport
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(
			XPBarContainer,
			50 // Z-order (below action bar)
		);
	}

	UE_LOG(LogWowGameplay, Log, TEXT("Created XP bar widget"));
}

void AWowGameplayController::UpdateXPBar()
{
	if (!ConnectionManager || !XPBar.IsValid() || !XPText.IsValid())
	{
		return;
	}

	// Get local player entity
	const FWowEntity* PlayerEntity = ConnectionManager->PacketHandler.EntityManager.GetLocalPlayer();
	if (!PlayerEntity)
	{
		return;
	}

	uint32 CurrentXP = PlayerEntity->GetField(PlayerField::XP);
	uint32 NextLevelXP = PlayerEntity->GetField(PlayerField::NEXT_LEVEL_XP);
	int32 Level = PlayerEntity->GetLevel();

	// Track XP changes and fire events
	static uint32 LastKnownXP = 0;
	if (CurrentXP != LastKnownXP && LastKnownXP > 0)
	{
		FireUIEvent(TEXT("PLAYER_XP_UPDATE"));
		LastKnownXP = CurrentXP;
	}
	else if (LastKnownXP == 0)
	{
		LastKnownXP = CurrentXP;
	}

	// Handle max level case
	if (NextLevelXP == 0 || Level >= 80) // Max level for 3.3.5a
	{
		XPBar->SetPercent(1.0f);
		XPText->SetText(FText::FromString(TEXT("Max Level")));
	}
	else
	{
		// Calculate percentage
		float Percent = NextLevelXP > 0 ? static_cast<float>(CurrentXP) / static_cast<float>(NextLevelXP) : 0.0f;
		int32 PercentInt = FMath::RoundToInt(Percent * 100.0f);

		XPBar->SetPercent(Percent);

		FString XPString = FString::Printf(TEXT("XP: %u / %u (%d%%)"), CurrentXP, NextLevelXP, PercentInt);
		XPText->SetText(FText::FromString(XPString));
	}
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

void AWowGameplayController::CreateTargetFrame()
{
	if (!ConnectionManager || TargetFrameText.IsValid())
	{
		return;
	}

	// Target name text
	TargetFrameText = SNew(STextBlock)
		.Text(FText::GetEmpty())
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		.ColorAndOpacity(FLinearColor::White);

	// Target health bar
	TargetHealthBar = SNew(SProgressBar)
		.Percent(1.0f)
		.FillColorAndOpacity(FLinearColor::Red)
		.BackgroundImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"));

	// Target health text
	TargetHealthText = SNew(STextBlock)
		.Text(FText::GetEmpty())
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
		.ColorAndOpacity(FLinearColor::White)
		.Justification(ETextJustify::Center);

	// Player health bar
	PlayerHealthBar = SNew(SProgressBar)
		.Percent(1.0f)
		.FillColorAndOpacity(FLinearColor::Green)
		.BackgroundImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"));

	PlayerHealthText = SNew(STextBlock)
		.Text(FText::GetEmpty())
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
		.ColorAndOpacity(FLinearColor::White)
		.Justification(ETextJustify::Center);

	PlayerNameText = SNew(STextBlock)
		.Text(FText::FromString(TEXT("Player")))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		.ColorAndOpacity(FLinearColor(0.5f, 1.0f, 0.5f));

	// Initialize aura display bars
	PlayerBuffBar = SNew(SHorizontalBox);
	TargetBuffBar = SNew(SHorizontalBox);

	if (GEngine && GEngine->GameViewport)
	{
		// Target frame — top center
		TSharedRef<SWidget> TargetContainer =
			SNew(SOverlay)
			.Visibility(EVisibility::SelfHitTestInvisible)
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(0, 20, 0, 0)
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
				.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.Padding(FMargin(8, 4))
				.Visibility_Lambda([this]() { return TargetGuid != 0 ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; })
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
					[
						TargetFrameText.ToSharedRef()
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).WidthOverride(200).HeightOverride(16)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								TargetHealthBar.ToSharedRef()
							]
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
							[
								TargetHealthText.ToSharedRef()
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
					[
						SNew(SBox).WidthOverride(200).HeightOverride(20)
						[
							TargetBuffBar.ToSharedRef()
						]
					]
				]
			];

		GEngine->GameViewport->AddViewportWidgetContent(TargetContainer, 80);

		// Player frame — top left
		TSharedRef<SWidget> PlayerContainer =
			SNew(SOverlay)
			.Visibility(EVisibility::SelfHitTestInvisible)
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(20, 20, 0, 0)
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f))
				.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
				.Padding(FMargin(8, 4))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)
					[
						PlayerNameText.ToSharedRef()
					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox).WidthOverride(200).HeightOverride(16)
						[
							SNew(SOverlay)
							+ SOverlay::Slot()
							[
								PlayerHealthBar.ToSharedRef()
							]
							+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
							[
								PlayerHealthText.ToSharedRef()
							]
						]
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
					[
						SNew(SBox).WidthOverride(200).HeightOverride(20)
						[
							PlayerBuffBar.ToSharedRef()
						]
					]
				]
			];

		GEngine->GameViewport->AddViewportWidgetContent(PlayerContainer, 80);

		UE_LOG(LogWowGameplay, Log, TEXT("Created player and target frame widgets"));
	}
}

void AWowGameplayController::UpdateTargetFrame()
{
	if (!ConnectionManager) return;

	// Update player health bar
	if (PlayerHealthBar.IsValid())
	{
		const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;
		const FWowEntity* LocalEntity = ConnectionManager->PacketHandler.EntityManager.Find(LocalGuid);
		if (LocalEntity && LocalEntity->GetMaxHealth() > 0)
		{
			int32 HP = LocalEntity->GetHealth();
			int32 MaxHP = LocalEntity->GetMaxHealth();
			float Pct = FMath::Clamp((float)HP / (float)MaxHP, 0.0f, 1.0f);
			PlayerHealthBar->SetPercent(Pct);
			if (PlayerHealthText.IsValid())
			{
				PlayerHealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), HP, MaxHP)));
			}
		}
	}

	if (!TargetFrameText.IsValid()) return;

	// If no target, bars remain but target section hides via visibility lambda
	if (TargetGuid == 0) return;

	// Find the target entity
	const FWowEntity* TargetEntity = ConnectionManager->PacketHandler.EntityManager.Find(TargetGuid);
	if (!TargetEntity) return;

	// Update target health bar
	if (TargetHealthBar.IsValid())
	{
		int32 HP = TargetEntity->GetHealth();
		int32 MaxHP = TargetEntity->GetMaxHealth();
		float Pct = (MaxHP > 0) ? FMath::Clamp((float)HP / (float)MaxHP, 0.0f, 1.0f) : 1.0f;
		TargetHealthBar->SetPercent(Pct);
		if (TargetHealthText.IsValid())
		{
			TargetHealthText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), HP, MaxHP)));
		}
	}

	// Get target name
	FString TargetName = TEXT("Unknown");
	if (TargetEntity->IsPlayer())
	{
		// For players, try to get name from cache (would need proper name resolution)
		TargetName = FString::Printf(TEXT("Player %llu"), TargetGuid);
	}
	else if (TargetEntity->IsUnit())
	{
		// For creatures, try to get name from DBC
		TargetName = FString::Printf(TEXT("Creature %u"), TargetEntity->Entry);

		const FDbcStore& DbcStore = FDbcStore::Get();
		if (DbcStore.IsLoaded())
		{
			// Try to get creature name from DBC if available
			// Note: This assumes there's a CreatureTemplate DBC, adjust as needed
			TargetName = FString::Printf(TEXT("Creature %u"), TargetEntity->Entry);
		}
	}

	// Get health info
	int32 Health = TargetEntity->GetHealth();
	int32 MaxHealth = TargetEntity->GetMaxHealth();
	int32 Level = TargetEntity->GetLevel();

	// Create target display text
	FString TargetText = FString::Printf(TEXT("Target: %s Level %d HP: %d/%d"),
		*TargetName, Level, Health, MaxHealth);

	// Update the widget
	TargetFrameText->SetText(FText::FromString(TargetText));
	TargetFrameText->SetVisibility(EVisibility::SelfHitTestInvisible);

	// Color code based on target type
	FLinearColor TargetColor = FLinearColor::White;
	if (TargetEntity->IsPlayer())
	{
		TargetColor = FLinearColor::Green; // Friendly player
	}
	else if (TargetEntity->IsUnit())
	{
		// Simplified hostility check - assume creatures are hostile
		TargetColor = FLinearColor::Red; // Hostile creature
	}

	TargetFrameText->SetColorAndOpacity(TargetColor);
}

void AWowGameplayController::SetTarget(uint64 NewTargetGuid)
{
	uint64 OldTargetGuid = TargetGuid;
	TargetGuid = NewTargetGuid;

	// Fire target change event
	if (OldTargetGuid != NewTargetGuid)
	{
		FireUIEvent(TEXT("PLAYER_TARGET_CHANGED"));
	}

	// Update nameplate highlighting

	// Unhighlight previous target's nameplate
	if (OldTargetGuid != 0)
	{
		TObjectPtr<UWidgetComponent>* OldNameplatePtr = EntityNameplates.Find(OldTargetGuid);
		if (OldNameplatePtr && *OldNameplatePtr)
		{
			UWidgetComponent* NameplateComponent = *OldNameplatePtr;
			if (UWowNameplateWidget* NameplateWidget = Cast<UWowNameplateWidget>(NameplateComponent->GetUserWidgetObject()))
			{
				// Reset nameplate to normal state (would need to add this method to nameplate)
				// For now, just change the scale back to normal
				NameplateComponent->SetWorldScale3D(FVector(1.0f, 1.0f, 1.0f));
			}
		}
	}

	// Highlight new target's nameplate
	if (NewTargetGuid != 0)
	{
		TObjectPtr<UWidgetComponent>* NewNameplatePtr = EntityNameplates.Find(NewTargetGuid);
		if (NewNameplatePtr && *NewNameplatePtr)
		{
			UWidgetComponent* NameplateComponent = *NewNameplatePtr;
			if (UWowNameplateWidget* NameplateWidget = Cast<UWowNameplateWidget>(NameplateComponent->GetUserWidgetObject()))
			{
				// Highlight the nameplate (simple approach: scale it up slightly)
				NameplateComponent->SetWorldScale3D(FVector(1.2f, 1.2f, 1.2f));
			}
		}
	}

	// Send to server
	if (ConnectionManager)
	{
		ConnectionManager->SendSetSelection(static_cast<int64>(TargetGuid));
	}

	// Update highlight circle
	UpdateTargetHighlight();

	// Log the change
	if (NewTargetGuid != 0)
	{
		UE_LOG(LogWowGameplay, Log, TEXT("Targeted entity GUID: %llu"), NewTargetGuid);
	}
	else
	{
		UE_LOG(LogWowGameplay, Log, TEXT("Target cleared"));
	}
}

void AWowGameplayController::UpdateTargetHighlight()
{
	// Remove existing highlight
	if (TargetHighlightCircle)
	{
		TargetHighlightCircle->DestroyComponent();
		TargetHighlightCircle = nullptr;
	}

	if (TargetGuid == 0) return;

	// Find the target actor
	TObjectPtr<AActor>* TargetActorPtr = SpawnedEntityActors.Find(TargetGuid);
	if (!TargetActorPtr || !*TargetActorPtr) return;

	AActor* TargetActor = *TargetActorPtr;

	// Create a flat cylinder mesh as the highlight circle
	TargetHighlightCircle = NewObject<UStaticMeshComponent>(TargetActor, TEXT("TargetHighlight"));
	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder"));
	if (!CylinderMesh) return;

	TargetHighlightCircle->SetStaticMesh(CylinderMesh);
	TargetHighlightCircle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetHighlightCircle->CastShadow = false;
	TargetHighlightCircle->bReceivesDecals = false;

	// Determine color based on entity type
	const FWowEntity* Entity = ConnectionManager ?
		ConnectionManager->PacketHandler.EntityManager.Find(TargetGuid) : nullptr;

	FLinearColor HighlightColor = FLinearColor::Red;
	if (Entity && Entity->IsPlayer())
	{
		HighlightColor = FLinearColor::Green;
	}
	else if (Entity)
	{
		uint32 NpcFlags = Entity->GetField(UnitField::NPC_FLAGS);
		if (NpcFlags & WowNpcFlags::INTERACTABLE)
		{
			HighlightColor = FLinearColor::Green;
		}
	}

	// Build a translucent emissive material at runtime
#if WITH_EDITOR
	UMaterial* RingMat = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_TargetRing"));
	RingMat->SetShadingModel(MSM_Unlit);
	RingMat->BlendMode = BLEND_Translucent;

	auto* ColorConst = NewObject<UMaterialExpressionConstant3Vector>(RingMat);
	ColorConst->Constant = HighlightColor;
	RingMat->GetExpressionCollection().AddExpression(ColorConst);
	RingMat->GetEditorOnlyData()->EmissiveColor.Connect(0, ColorConst);

	auto* OpacityConst = NewObject<UMaterialExpressionConstant>(RingMat);
	OpacityConst->R = 0.35f;
	RingMat->GetExpressionCollection().AddExpression(OpacityConst);
	RingMat->GetEditorOnlyData()->Opacity.Connect(0, OpacityConst);

	RingMat->TwoSided = true;
	RingMat->PreEditChange(nullptr);
	RingMat->PostEditChange();

	TargetHighlightCircle->SetMaterial(0, RingMat);
#else
	// Non-editor: use basic material fallback (won't be translucent but will be visible)
	UMaterialInterface* BaseMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (BaseMat)
	{
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, TargetHighlightCircle);
		DynMat->SetVectorParameterValue(TEXT("Color"), HighlightColor);
		TargetHighlightCircle->SetMaterial(0, DynMat);
	}
#endif

	// Attach FIRST with relative transform, then set scale/location relative to parent
	TargetHighlightCircle->RegisterComponent();
	TargetHighlightCircle->AttachToComponent(TargetActor->GetRootComponent(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	TargetHighlightCircle->SetRelativeLocation(FVector(0.0f, 0.0f, -85.0f));
	TargetHighlightCircle->SetRelativeScale3D(FVector(1.5f, 1.5f, 0.01f));

	UE_LOG(LogWowGameplay, Log, TEXT("Created target highlight circle for entity %llu"), TargetGuid);
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
			// Wrap in positioned overlay that passes clicks through when not interacting
			TSharedRef<SWidget> TaxiMapContainer =
				SNew(SOverlay)
				.Visibility(EVisibility::SelfHitTestInvisible) // Pass clicks through to 3D world when taxi map is closed
				+ SOverlay::Slot()
				[
					TaxiMapWidget.ToSharedRef()
				];

			GEngine->GameViewport->AddViewportWidgetContent(
				TaxiMapContainer,
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

	UE_LOG(LogWowGameplay, Log, TEXT("CastSpellFromSlot: slot=%d packed=0x%08X actionId=%u type=%u"),
		SlotIndex, PackedAction, ActionId, ActionType);

	if (ActionId == 0) return;

	if (ActionType == 0) // Spell
	{
		// Auto-Attack (spell 6603) uses CMSG_ATTACKSWING, not CMSG_CAST_SPELL
		if (ActionId == 6603)
		{
			StartAutoAttack();
		}
		else
		{
			ConnectionManager->SendCastSpell(ActionId, static_cast<int64>(TargetGuid));
		}
		FireUIEvent(TEXT("ACTIONBAR_UPDATE_STATE"));
	}
	else if (ActionType == 128) // Item
	{
		UE_LOG(LogWowGameplay, Log, TEXT("CastSpellFromSlot: item use not implemented (item=%u)"), ActionId);
	}
	else if (ActionType == 64) // Macro
	{
		UE_LOG(LogWowGameplay, Log, TEXT("CastSpellFromSlot: macro not implemented (macro=%u)"), ActionId);
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

		// Return to idle animation for local player
		if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn()))
		{
			if (UWowAnimationController* AnimController = PlayerChar->AnimationController)
			{
				AnimController->PlayAnimationById(EWowAnimId::Stand, true);
				UE_LOG(LogWowGameplay, Log, TEXT("Returning local player to idle animation after spell failure"));
			}
		}

		// Remove hand glow effects
		RemoveCastGlowEffects();

		// Fire spell failure event
		FireUIEvent(TEXT("UNIT_SPELLCAST_FAILED"), {TEXT("player")});

		// TODO: Show error message based on failure reason
	}
	else
	{
		// Return other entity to idle animation
		if (TObjectPtr<AActor>* FoundActor = SpawnedEntityActors.Find(CasterGuid))
		{
			if (AActor* CasterActor = FoundActor->Get())
			{
				if (UWowAnimationController* AnimController = FWowCharacterBuilder::GetAnimationController(CasterActor))
				{
					AnimController->PlayAnimationById(EWowAnimId::Stand, true);
					UE_LOG(LogWowGameplay, Log, TEXT("Returning entity %llu to idle animation after spell failure"), CasterGuid);
				}
			}
		}

		UE_LOG(LogWowGameplay, Log, TEXT("Entity %llu spell %u failed"), CasterGuid, SpellId);

		// Remove hand glow effects for other entities
		RemoveCastGlowEffects();
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

	// Play attack animation on the attacker
	const uint64 LocalGuid2 = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;
	if (AttackerGuid == LocalGuid2)
	{
		// Local player attack animation
		if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn()))
		{
			if (UWowAnimationController* AnimController = PlayerChar->AnimationController)
			{
				AnimController->PlayAttackAnimation();
			}
		}
	}
	else
	{
		// Other entity attack animation
		if (TObjectPtr<AActor>* AttackerActorPtr = SpawnedEntityActors.Find(AttackerGuid))
		{
			if (AActor* AttackerActor = AttackerActorPtr->Get())
			{
				if (UWowAnimationController* AnimController = FWowCharacterBuilder::GetAnimationController(AttackerActor))
				{
					AnimController->PlayAttackAnimation();
				}
			}
		}
	}

	// Play wound/hit reaction animation on the victim
	if (Damage > 0 && VictimGuid != LocalGuid2)
	{
		if (TObjectPtr<AActor>* VictimActorPtr = SpawnedEntityActors.Find(VictimGuid))
		{
			if (AActor* VictimActor = VictimActorPtr->Get())
			{
				if (UWowAnimationController* AnimController = FWowCharacterBuilder::GetAnimationController(VictimActor))
				{
					AnimController->PlayWoundAnimation();
				}
			}
		}
	}
	else if (Damage > 0 && VictimGuid == LocalGuid2)
	{
		// Local player hit reaction
		if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn()))
		{
			if (UWowAnimationController* AnimController = PlayerChar->AnimationController)
			{
				AnimController->PlayWoundAnimation();
			}
		}
	}

	// Check if our auto-attack target died (simplified check)
	if (bIsAutoAttacking && VictimGuid == AutoAttackTargetGuid)
	{
		// In a real implementation, we'd check if target health reached 0
		// For now, we'll keep auto-attacking until manually stopped
	}

	// Fire combat log event
	FireUIEvent(TEXT("COMBAT_LOG_EVENT"));
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

	// Fire appropriate chat events based on message type
	switch (Type)
	{
		case 0: // CHAT_MSG_SAY
			FireUIEvent(TEXT("CHAT_MSG_SAY"));
			break;
		case 1: // CHAT_MSG_PARTY
			FireUIEvent(TEXT("CHAT_MSG_PARTY"));
			break;
		case 2: // CHAT_MSG_RAID
			FireUIEvent(TEXT("CHAT_MSG_RAID"));
			break;
		case 3: // CHAT_MSG_GUILD
			FireUIEvent(TEXT("CHAT_MSG_GUILD"));
			break;
		case 6: // CHAT_MSG_YELL
			FireUIEvent(TEXT("CHAT_MSG_YELL"));
			break;
		case 7: // CHAT_MSG_WHISPER
			FireUIEvent(TEXT("CHAT_MSG_WHISPER"));
			break;
		default:
			// Fire generic chat event for other types
			FireUIEvent(TEXT("CHAT_MSG_SAY"));
			break;
	}
}

void AWowGameplayController::OnEmote(uint64 EntityGuid, uint32 EmoteId)
{
	if (!ConnectionManager) return;

	UE_LOG(LogWowGameplay, Log, TEXT("Entity %llu performed emote %u"), EntityGuid, EmoteId);

	// Find the entity that performed the emote
	const FWowEntity* Entity = ConnectionManager->PacketHandler.EntityManager.Find(EntityGuid);
	if (!Entity)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("OnEmote: Entity %llu not found"), EntityGuid);
		return;
	}

	// Try to find the associated character actor
	AActor* CharacterActor = nullptr;
	if (TObjectPtr<AActor>* ActorPtr = SpawnedEntityActors.Find(EntityGuid))
	{
		CharacterActor = *ActorPtr;
	}

	// If this is the local player, use the player character pawn instead
	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;
	if (EntityGuid == LocalGuid)
	{
		if (APawn* LocalPawn = GetPawn())
		{
			if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(LocalPawn))
			{
				PlayEmoteAnimation(PlayerChar, EmoteId);
				return;
			}
		}
	}

	// For other entities, play the animation on their character actor
	if (CharacterActor)
	{
		PlayEmoteAnimation(CharacterActor, EmoteId);
	}
}

void AWowGameplayController::PlayEmoteAnimation(AActor* CharacterActor, uint32 EmoteId)
{
	if (!CharacterActor)
	{
		return;
	}

	// Get the animation controller using the standard approach
	UWowAnimationController* AnimController = nullptr;

	// Try AWowPlayerCharacter first (for local player)
	if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(CharacterActor))
	{
		AnimController = PlayerChar->AnimationController;
	}
	else
	{
		// For spawned entities, use FWowCharacterBuilder
		AnimController = FWowCharacterBuilder::GetAnimationController(CharacterActor);
	}

	if (!AnimController)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("PlayEmoteAnimation: No animation controller found on character"));
		return;
	}

	// Map emote IDs to animation IDs
	EWowAnimId AnimId = GetAnimationForEmoteId(EmoteId);
	if (AnimId != EWowAnimId::Stand) // Stand is our "invalid" fallback
	{
		AnimController->PlayAnimationById(AnimId, false); // Don't loop emotes
		UE_LOG(LogWowGameplay, Log, TEXT("Playing emote animation %u -> %u on character"), EmoteId, static_cast<uint32>(AnimId));
	}
	else
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Unknown emote ID: %u"), EmoteId);
	}
}

EWowAnimId AWowGameplayController::GetAnimationForEmoteId(uint32 EmoteId)
{
	// Map common emote IDs to WoW animation IDs
	// Reference: https://wowdev.wiki/Emotes.dbc
	switch (EmoteId)
	{
		case 10:  return EWowAnimId::Dance;      // Dance
		case 2:   return EWowAnimId::EmoteBow;   // Bow
		case 4:   return EWowAnimId::EmoteTalk;  // Cheer (use talk for now)
		case 60:  return EWowAnimId::EmoteTalk;  // Talk
		case 61:  return EWowAnimId::EmoteEat;   // Eat
		case 69:  return EWowAnimId::SitGround;  // Sit
		case 0:   return EWowAnimId::Stand;      // Stand
		case 72:  return EWowAnimId::Kneel;      // Kneel
		case 71:  return EWowAnimId::Sleep;      // Sleep

		// Add more mappings as needed
		default:
			// For unknown emotes, try to use the EmoteId as a direct animation ID
			// if it's within reasonable bounds
			if (EmoteId <= 200)
			{
				return static_cast<EWowAnimId>(EmoteId);
			}
			return EWowAnimId::Stand; // Fallback
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

	// Detect death: health dropped FROM positive TO 0 on a non-player entity
	// Only trigger if OldHealth was positive — this ensures we actually saw it alive first
	if (NewHealth <= 0 && OldHealth > 0 && Entity.Guid != LocalGuid && !DeadEntityGuids.Contains(Entity.Guid))
	{
		OnEntityDeath(Entity.Guid);

		// Fire health event if it's our target
		if (Entity.Guid == TargetGuid)
		{
			FireUIEvent(TEXT("UNIT_HEALTH"), {TEXT("target")});
		}
	}
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

void AWowGameplayController::OnChatInputModeChanged(bool bChatActive)
{
	if (bChatActive && FSlateApplication::IsInitialized())
	{
		ApplyWowGameAndUiInputMode(this, FSlateApplication::Get().GetKeyboardFocusedWidget());
		return;
	}

	ApplyWowGameAndUiInputMode(this, GetWowGameViewportWidget());
	FocusWowGameViewport();
}

void AWowGameplayController::OnBagKey()
{
	// Use FrameXML ToggleBackpack — no native fallback
	if (UIManager && UIManager->GetLuaVM() && UIManager->GetLuaVM()->IsInitialized())
	{
		UIManager->GetLuaVM()->ExecuteString(TEXT("ToggleBackpack()"), TEXT("=keybind_B"));
	}
}

void AWowGameplayController::OnCharacterKey()
{
	// Use FrameXML ToggleCharacter — no native fallback
	if (UIManager && UIManager->GetLuaVM() && UIManager->GetLuaVM()->IsInitialized())
	{
		UIManager->GetLuaVM()->ExecuteString(TEXT("ToggleCharacter('PaperDollFrame')"), TEXT("=keybind_C"));
	}
}

void AWowGameplayController::OnQuestLogKey()
{
	if (!QuestLogWidget.IsValid())
	{
		CreateQuestLog();
	}

	if (QuestLogWidget.IsValid())
	{
		QuestLogWidget->ToggleVisibility();
	}
}

void AWowGameplayController::CreateQuestLog()
{
	if (QuestLogWidget.IsValid())
	{
		return; // Already created
	}

	// Get the packet handler for quest data
	FWowPacketHandler* PacketHandler = nullptr;
	if (ConnectionManager)
	{
		PacketHandler = &ConnectionManager->PacketHandler;
	}

	if (!PacketHandler)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Cannot create quest log: no packet handler available"));
		return;
	}

	// Create the quest log widget
	QuestLogWidget = SNew(SWowQuestLog, PacketHandler);

	// Add to viewport
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(
			QuestLogWidget.ToSharedRef(),
			10 // Z-order: above most UI but below tooltips
		);
	}

	// Position the quest log on screen (right side, below minimap)
	if (QuestLogWidget.IsValid())
	{
		// Set initial position
		FVector2D ViewportSize;
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->GetViewportSize(ViewportSize);

			// Position on right side of screen
			FVector2D QuestLogPos(ViewportSize.X - 370.0f, 100.0f);

			// This will be handled by Slate anchoring in the widget itself
			// For now, the widget uses a fixed size and will appear centered
		}
	}
}

void AWowGameplayController::OnTalentKey()
{
	if (!TalentWindowWidget.IsValid())
	{
		CreateTalentWindow();
	}

	if (TalentWindowWidget.IsValid())
	{
		TalentWindowWidget->ToggleVisibility();
	}
}

void AWowGameplayController::CreateTalentWindow()
{
	if (TalentWindowWidget.IsValid())
	{
		return; // Already created
	}

	if (!ConnectionManager)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Cannot create talent window: no connection manager available"));
		return;
	}

	// Create the talent window widget
	TalentWindowWidget = SNew(SWowTalentWindow, ConnectionManager);

	// Add to viewport
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(
			TalentWindowWidget.ToSharedRef(),
			10 // Z-order: above most UI but below tooltips
		);
	}
}

void AWowGameplayController::CreateQuestTracker()
{
	if (QuestTrackerWidget.IsValid())
	{
		return; // Already created
	}

	// Create the quest tracker widget - positioned on the right side of screen
	TSharedRef<SWidget> QuestTrackerContainer =
		SNew(SOverlay)
		.Visibility(EVisibility::SelfHitTestInvisible) // Pass clicks through to 3D world
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		.Padding(10.0f, 250.0f, 10.0f, 10.0f) // Position below minimap
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("BlackBrush"))
			.BorderBackgroundColor(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f))
			.Padding(10.0f)
			[
				SNew(SBox)
				.WidthOverride(200.0f)
				.MaxDesiredHeight(400.0f)
				[
					SNew(SVerticalBox)

					// Title
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 5.0f)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("Quest Objectives")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						.ColorAndOpacity(FLinearColor(1.0f, 0.8f, 0.0f, 1.0f)) // Gold text
						.Justification(ETextJustify::Center)
					]

					// Quest list container
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(QuestTrackerWidget, SVerticalBox)
					]
				]
			]
		];

	// Add to viewport
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(
			QuestTrackerContainer,
			5 // Z-order: below minimap but above 3D world
		);
	}

	// Trigger initial update
	UpdateQuestTracker();
}

void AWowGameplayController::UpdateQuestTracker()
{
	if (!QuestTrackerWidget.IsValid() || !ConnectionManager)
	{
		return;
	}

	// Clear existing quest entries
	QuestTrackerWidget->ClearChildren();

	// Get quest log from packet handler
	const TArray<FWowQuestLogEntry>& QuestLog = ConnectionManager->PacketHandler.QuestLog;

	int32 QuestsShown = 0;
	const int32 MaxQuestsToShow = 5;

	for (const FWowQuestLogEntry& QuestEntry : QuestLog)
	{
		if (QuestsShown >= MaxQuestsToShow)
		{
			break;
		}

		// Get quest name (simplified - in a real implementation, you'd look this up from Quest.dbc)
		FString QuestName = FString::Printf(TEXT("Quest %u"), QuestEntry.QuestId);

		// Determine completion status
		bool bIsComplete = (QuestEntry.State == 1); // 1 = complete
		FLinearColor QuestColor = bIsComplete ? FLinearColor::Green : FLinearColor::White;

		// Add quest title
		QuestTrackerWidget->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 5.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(QuestName))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			.ColorAndOpacity(QuestColor)
		];

		// Add quest objectives
		for (int32 i = 0; i < QuestEntry.Objectives.Num(); ++i)
		{
			const FWowQuestObjective& Objective = QuestEntry.Objectives[i];

			if (Objective.Required > 0) // Only show objectives that have requirements
			{
				bool bObjectiveComplete = (Objective.Count >= Objective.Required);
				FLinearColor ObjectiveColor = bObjectiveComplete ? FLinearColor::Green : FLinearColor::White;

				FString ObjectiveText;
				if (Objective.CreatureOrGOId > 0)
				{
					ObjectiveText = FString::Printf(TEXT("- Kill/Collect: %u / %u"),
						Objective.Count, Objective.Required);
				}
				else
				{
					ObjectiveText = FString::Printf(TEXT("- Objective %d: %u / %u"),
						i + 1, Objective.Count, Objective.Required);
				}

				QuestTrackerWidget->AddSlot()
				.AutoHeight()
				.Padding(10.0f, 0.0f, 0.0f, 1.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(ObjectiveText))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.ColorAndOpacity(ObjectiveColor)
				];
			}
		}

		QuestsShown++;
	}

	// If no quests, show a placeholder
	if (QuestLog.Num() == 0)
	{
		QuestTrackerWidget->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 10.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(TEXT("No active quests")))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f))
			.Justification(ETextJustify::Center)
		];
	}
}

void AWowGameplayController::OnPlayerInventoryUpdated()
{
	if (UIManager)
	{
		UIManager->UpdateInventory();
	}

	// Fire inventory events
	FireUIEvent(TEXT("BAG_UPDATE"));
	FireUIEvent(TEXT("PLAYER_MONEY"));
	FireUIEvent(TEXT("PLAYER_EQUIPMENT_CHANGED"));
	FireUIEvent(TEXT("UPDATE_INVENTORY_DURABILITY"));
}

void AWowGameplayController::OnInitialSpells(const TArray<uint32>& SpellIds)
{
	UE_LOG(LogWowGameplay, Log, TEXT("Received initial spells: %d spells"), SpellIds.Num());

	// Fire spell book events
	FireUIEvent(TEXT("SPELLS_CHANGED"));
	FireUIEvent(TEXT("LEARNED_SPELL_IN_TAB"));
}

void AWowGameplayController::OnActionButtonsUpdated()
{
	UE_LOG(LogWowGameplay, Log, TEXT("Action buttons updated"));

	if (ActionBarWidget.IsValid())
	{
		ActionBarWidget->RefreshActionButtons();
	}

	// Fire action bar events
	for (int32 SlotIndex = 0; SlotIndex < 12; ++SlotIndex)
	{
		FireUIEvent(TEXT("ACTIONBAR_SLOT_CHANGED"), {FString::Printf(TEXT("%d"), SlotIndex + 1)});
	}
	FireUIEvent(TEXT("ACTIONBAR_UPDATE_STATE"));
}

void AWowGameplayController::SpawnSpellVisualEffect(uint64 CasterGuid, uint64 SpellTargetGuid, uint32 SpellId, bool bIsMissile)
{
	// Check if deferred DBCs are loaded
	const FDbcStore& DbcStore = FDbcStore::Get();
	if (!DbcStore.IsDeferredLoaded())
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("SpawnSpellVisualEffect: Deferred DBCs not loaded yet, spell %u"), SpellId);
		return;
	}

	// 1. Look up spell in SpellDbc → get SpellVisual[0]
	const auto* SpellEntry = DbcStore.Spells().GetById(SpellId);
	if (!SpellEntry)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("SpawnSpellVisualEffect: Spell %u not found in DBC"), SpellId);
		return;
	}

	uint32 SpellVisualId = SpellEntry->SpellVisual[0];
	if (SpellVisualId == 0)
	{
		UE_LOG(LogWowGameplay, Log, TEXT("SpawnSpellVisualEffect: Spell %u has no visual (SpellVisual[0] = 0)"), SpellId);
		return;
	}

	// 2. Look up SpellVisual → get CastKit, ImpactKit, HasMissile, MissileModel
	const auto* SpellVisualEntry = DbcStore.SpellVisuals().GetById(SpellVisualId);
	if (!SpellVisualEntry)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("SpawnSpellVisualEffect: SpellVisual %u not found in DBC"), SpellVisualId);
		return;
	}

	UE_LOG(LogWowGameplay, Log, TEXT("SpawnSpellVisualEffect: Spell %u (%s) → SpellVisual %u"),
		SpellId, *SpellEntry->SpellName, SpellVisualId);
	UE_LOG(LogWowGameplay, Log, TEXT("  CastKit=%u, ImpactKit=%u, HasMissile=%u, MissileModel=%d"),
		SpellVisualEntry->CastKit, SpellVisualEntry->ImpactKit, SpellVisualEntry->HasMissile, SpellVisualEntry->MissileModel);

	// 3. Process CastKit if present
	if (SpellVisualEntry->CastKit > 0)
	{
		const auto* CastKitEntry = DbcStore.SpellVisualKits().GetById(SpellVisualEntry->CastKit);
		if (CastKitEntry)
		{
			UE_LOG(LogWowGameplay, Log, TEXT("  CastKit %u: LeftHand=%u, RightHand=%u, Base=%u, Chest=%u"),
				SpellVisualEntry->CastKit, CastKitEntry->LeftHandEffectID, CastKitEntry->RightHandEffectID,
				CastKitEntry->BaseEffectID, CastKitEntry->ChestEffectID);

			// Look up effect names
			auto LogEffectName = [&](uint32 EffectId, const FString& SlotName)
			{
				if (EffectId > 0)
				{
					const auto* EffectEntry = DbcStore.SpellVisualEffectNames().GetById(EffectId);
					if (EffectEntry)
					{
						UE_LOG(LogWowGameplay, Log, TEXT("    %s Effect %u: %s → %s"),
							*SlotName, EffectId, *EffectEntry->Name, *EffectEntry->FilePath);
					}
					else
					{
						UE_LOG(LogWowGameplay, Warning, TEXT("    %s Effect %u: Not found in SpellVisualEffectName DBC"), *SlotName, EffectId);
					}
				}
			};

			LogEffectName(CastKitEntry->LeftHandEffectID, TEXT("LeftHand"));
			LogEffectName(CastKitEntry->RightHandEffectID, TEXT("RightHand"));
			LogEffectName(CastKitEntry->BaseEffectID, TEXT("Base"));
			LogEffectName(CastKitEntry->ChestEffectID, TEXT("Chest"));
			LogEffectName(CastKitEntry->HeadEffectID, TEXT("Head"));
		}
		else
		{
			UE_LOG(LogWowGameplay, Warning, TEXT("  CastKit %u not found in SpellVisualKit DBC"), SpellVisualEntry->CastKit);
		}
	}

	// 4. Process ImpactKit if present
	if (SpellVisualEntry->ImpactKit > 0)
	{
		const auto* ImpactKitEntry = DbcStore.SpellVisualKits().GetById(SpellVisualEntry->ImpactKit);
		if (ImpactKitEntry)
		{
			UE_LOG(LogWowGameplay, Log, TEXT("  ImpactKit %u: LeftHand=%u, RightHand=%u, Base=%u, Chest=%u"),
				SpellVisualEntry->ImpactKit, ImpactKitEntry->LeftHandEffectID, ImpactKitEntry->RightHandEffectID,
				ImpactKitEntry->BaseEffectID, ImpactKitEntry->ChestEffectID);
		}
		else
		{
			UE_LOG(LogWowGameplay, Warning, TEXT("  ImpactKit %u not found in SpellVisualKit DBC"), SpellVisualEntry->ImpactKit);
		}
	}
}

void AWowGameplayController::SpawnSpellMissile(uint64 CasterGuid, uint64 SpellTargetGuid, uint32 SpellId)
{
	// Check if deferred DBCs are loaded
	const FDbcStore& DbcStore = FDbcStore::Get();
	if (!DbcStore.IsDeferredLoaded())
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("SpawnSpellMissile: Deferred DBCs not loaded yet, spell %u"), SpellId);
		return;
	}

	// Look up spell visual data
	const auto* SpellEntry = DbcStore.Spells().GetById(SpellId);
	if (!SpellEntry)
	{
		return;
	}

	uint32 SpellVisualId = SpellEntry->SpellVisual[0];
	if (SpellVisualId == 0)
	{
		return;
	}

	const auto* SpellVisualEntry = DbcStore.SpellVisuals().GetById(SpellVisualId);
	if (!SpellVisualEntry || !SpellVisualEntry->HasMissile)
	{
		// No missile for this spell
		return;
	}

	// Get caster position
	FVector CasterLocation = FVector::ZeroVector;
	bool bFoundCasterLocation = false;

	if (!ConnectionManager)
	{
		return;
	}

	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;
	if (CasterGuid == LocalGuid)
	{
		// Local player casting
		if (APawn* LocalPawn = GetPawn())
		{
			CasterLocation = LocalPawn->GetActorLocation();
			bFoundCasterLocation = true;
		}
	}
	else
	{
		// Other entity casting
		if (TObjectPtr<AActor>* FoundActor = SpawnedEntityActors.Find(CasterGuid))
		{
			if (AActor* CasterActor = FoundActor->Get())
			{
				CasterLocation = CasterActor->GetActorLocation();
				bFoundCasterLocation = true;
			}
		}
	}

	if (!bFoundCasterLocation)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("SpawnSpellMissile: Could not find caster %llu location"), CasterGuid);
		return;
	}

	// Get target position
	FVector TargetLocation = CasterLocation + FVector(300.0f, 0.0f, 0.0f); // Default forward target
	bool bFoundTargetLocation = false;

	// Try to get target GUID from caster entity's target field
	if (SpellTargetGuid == 0)
	{
		// Try to resolve target from entity manager
		auto& EntityManager = ConnectionManager->PacketHandler.EntityManager;
		if (const FWowEntity* CasterEntity = EntityManager.Find(CasterGuid))
		{
			// Try to get the target field from the entity
			SpellTargetGuid = CasterEntity->GetField64(UnitField::TARGET);
		}
	}

	// Get target location
	if (SpellTargetGuid != 0)
	{
		if (SpellTargetGuid == LocalGuid)
		{
			// Target is local player
			if (APawn* LocalPawn = GetPawn())
			{
				TargetLocation = LocalPawn->GetActorLocation();
				bFoundTargetLocation = true;
			}
		}
		else
		{
			// Target is another entity
			if (TObjectPtr<AActor>* FoundActor = SpawnedEntityActors.Find(SpellTargetGuid))
			{
				if (AActor* TargetActor = FoundActor->Get())
				{
					TargetLocation = TargetActor->GetActorLocation();
					bFoundTargetLocation = true;
				}
			}
		}
	}

	if (!bFoundTargetLocation)
	{
		UE_LOG(LogWowGameplay, Log, TEXT("SpawnSpellMissile: No specific target found for spell %u, using forward direction"), SpellId);
	}

	// Determine spell school color
	FLinearColor SpellColor = GetSpellSchoolColor(SpellEntry->SchoolMask);

	// Spawn the missile
	if (UWorld* World = GetWorld())
	{
		// Adjust missile spawn position slightly above caster
		FVector MissileStartLocation = CasterLocation + FVector(0.0f, 0.0f, 150.0f);
		FVector MissileTargetLocation = TargetLocation + FVector(0.0f, 0.0f, 100.0f);

		AWowSpellMissile* Missile = World->SpawnActor<AWowSpellMissile>();
		if (Missile)
		{
			Missile->Initialize(MissileStartLocation, MissileTargetLocation, SpellColor, 1.0f);
			UE_LOG(LogWowGameplay, Log, TEXT("Spawned spell missile for spell %u (%s) from caster %llu to target %llu"),
				SpellId, *SpellEntry->SpellName, CasterGuid, SpellTargetGuid);
		}
	}
}

FLinearColor AWowGameplayController::GetSpellSchoolColor(uint32 SchoolMask) const
{
	// Spell school color mapping based on SchoolMask - vibrant colors for missile visibility
	if (SchoolMask & 0x04) // Fire
		return FLinearColor(1.0f, 0.4f, 0.0f); // bright orange
	else if (SchoolMask & 0x10) // Frost
		return FLinearColor(0.3f, 0.7f, 1.0f); // ice blue
	else if (SchoolMask & 0x08) // Nature
		return FLinearColor(0.2f, 1.0f, 0.2f); // bright green
	else if (SchoolMask & 0x20) // Shadow
		return FLinearColor(0.6f, 0.0f, 1.0f); // purple
	else if (SchoolMask & 0x40) // Arcane
		return FLinearColor(1.0f, 0.3f, 1.0f); // pink-purple
	else if (SchoolMask & 0x02) // Holy
		return FLinearColor(1.0f, 1.0f, 0.3f); // golden yellow
	else // Physical or unknown
		return FLinearColor(1.0f, 1.0f, 1.0f); // white
}

void AWowGameplayController::SpawnCastGlowEffects(uint64 CasterGuid, uint32 SpellId)
{
	// First clear any existing effects
	RemoveCastGlowEffects();

	if (!ConnectionManager) return;

	// Check if deferred DBCs are loaded
	const FDbcStore& DbcStore = FDbcStore::Get();
	if (!DbcStore.IsDeferredLoaded())
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("SpawnCastGlowEffects: Deferred DBCs not loaded yet, spell %u"), SpellId);
		return;
	}

	// Get spell school color
	const auto* SpellEntry = DbcStore.Spells().GetById(SpellId);
	if (!SpellEntry)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("SpawnCastGlowEffects: Spell %u not found in DBC"), SpellId);
		return;
	}

	FLinearColor SpellColor = GetSpellSchoolColor(SpellEntry->SchoolMask);

	// Find the caster actor
	AActor* CasterActor = nullptr;
	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;

	if (CasterGuid == LocalGuid)
	{
		// Local player
		CasterActor = GetPawn();
	}
	else
	{
		// Other entity
		if (TObjectPtr<AActor>* FoundActor = SpawnedEntityActors.Find(CasterGuid))
		{
			CasterActor = FoundActor->Get();
		}
	}

	if (!CasterActor)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("SpawnCastGlowEffects: Could not find caster actor for GUID %llu"), CasterGuid);
		return;
	}

	// Find skeletal mesh component
	USkeletalMeshComponent* MeshComp = CasterActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!MeshComp)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("SpawnCastGlowEffects: No skeletal mesh found on caster actor"));
		return;
	}

	// Get world
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Define hand positions (approximate if bone lookup fails)
	TArray<FVector> HandPositions;

	// Try to find hand bones - WoW M2 models use bone indices
	// For WoW character models, try to find reasonable bone positions for hands
	bool bFoundBones = false;

	// Try a wider range of bone indices to find valid hand positions
	// We'll take any non-zero bone transforms that could represent hands
	for (int32 BoneIndex = 5; BoneIndex < 30 && HandPositions.Num() < 2; ++BoneIndex)
	{
		if (MeshComp->GetNumBones() > BoneIndex)
		{
			FTransform BoneTransform = MeshComp->GetBoneTransform(BoneIndex);
			FVector BoneLocation = BoneTransform.GetLocation();

			// Check if this bone is in a reasonable position for a hand (not at origin, not too high/low)
			if (!BoneLocation.IsNearlyZero() && BoneLocation.Z > CasterActor->GetActorLocation().Z - 100.0f &&
			    BoneLocation.Z < CasterActor->GetActorLocation().Z + 200.0f)
			{
				HandPositions.Add(BoneLocation);
				bFoundBones = true;
				UE_LOG(LogWowGameplay, Log, TEXT("SpawnCastGlowEffects: Using bone %d at location (%f,%f,%f)"),
				       BoneIndex, BoneLocation.X, BoneLocation.Y, BoneLocation.Z);
			}
		}
	}

	// If bone lookup failed, use approximate positions relative to actor
	if (!bFoundBones)
	{
		FVector ActorLocation = CasterActor->GetActorLocation();
		FVector ActorForward = CasterActor->GetActorForwardVector();
		FVector ActorRight = CasterActor->GetActorRightVector();

		// Approximate hand positions
		HandPositions.Add(ActorLocation + ActorRight * 40.0f + FVector(0, 0, 80));  // Right hand
		HandPositions.Add(ActorLocation + ActorRight * -40.0f + FVector(0, 0, 80)); // Left hand

		UE_LOG(LogWowGameplay, Log, TEXT("SpawnCastGlowEffects: Using approximate hand positions for spell %u"), SpellId);
	}
	else
	{
		UE_LOG(LogWowGameplay, Log, TEXT("SpawnCastGlowEffects: Found %d bone positions for hand lights"), HandPositions.Num());
	}

	// Create light components for each hand position
	for (const FVector& HandPosition : HandPositions)
	{
		UPointLightComponent* LightComp = NewObject<UPointLightComponent>(CasterActor);
		if (LightComp)
		{
			// Configure the light
			LightComp->SetLightColor(SpellColor);
			LightComp->SetIntensity(5000.0f);
			LightComp->SetAttenuationRadius(150.0f);
			LightComp->SetCastShadows(false);

			// Set position
			LightComp->SetWorldLocation(HandPosition);

			// Attach to the actor
			LightComp->AttachToComponent(CasterActor->GetRootComponent(),
				FAttachmentTransformRules::KeepWorldTransform);

			// Register and activate
			LightComp->RegisterComponent();

			// Store in our active effects array
			ActiveCastEffects.Add(LightComp);
		}
	}

	UE_LOG(LogWowGameplay, Log, TEXT("SpawnCastGlowEffects: Created %d hand glow effects for spell %u (caster %llu)"),
		ActiveCastEffects.Num(), SpellId, CasterGuid);
}

void AWowGameplayController::RemoveCastGlowEffects()
{
	// Destroy all active cast effect components
	for (UPointLightComponent* LightComp : ActiveCastEffects)
	{
		if (IsValid(LightComp))
		{
			LightComp->DestroyComponent();
		}
	}

	// Clear the array
	ActiveCastEffects.Empty();
}

void AWowGameplayController::CreateSpellbook()
{
	if (!ConnectionManager || SpellbookWidget.IsValid())
	{
		return;
	}

	// Create the spellbook widget
	// SpellbookWidget = SNew(SWowSpellbook)
	//	.ConnectionManager(ConnectionManager);

	// Add to viewport with proper positioning
	/*
	if (GEngine && GEngine->GameViewport)
	{
		// Wrap in overlay that allows interaction
		TSharedRef<SWidget> SpellbookContainer =
			SNew(SOverlay)
			.Visibility(EVisibility::SelfHitTestInvisible) // Pass clicks through when collapsed
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SpellbookWidget.ToSharedRef()
			];

		GEngine->GameViewport->AddViewportWidgetContent(
			SpellbookContainer,
			100 // High Z-order to appear on top
		);
	}

	UE_LOG(LogWowGameplay, Log, TEXT("Created spellbook widget"));
	*/
}

void AWowGameplayController::OnSpellbookKey()
{
	// Fire WoW Lua — ToggleSpellBook(BOOKTYPE_SPELL)
	if (UIManager && UIManager->GetLuaVM() && UIManager->GetLuaVM()->IsInitialized())
	{
		UIManager->GetLuaVM()->ExecuteString(TEXT("ToggleSpellBook(BOOKTYPE_SPELL)"), TEXT("=keybind_P"));
	}
}

void AWowGameplayController::UpdateAuraDisplay()
{
	if (!ConnectionManager) return;

	// Clear existing aura icons
	if (PlayerBuffBar.IsValid())
	{
		PlayerBuffBar->ClearChildren();
	}
	if (TargetBuffBar.IsValid())
	{
		TargetBuffBar->ClearChildren();
	}

	// Get the DBC store for spell lookups
	const FDbcStore& DbcStore = FDbcStore::Get();
	if (!DbcStore.IsDeferredLoaded())
	{
		// Skip aura display if spell DBC is not loaded yet
		return;
	}

	// Update player auras
	const uint64 LocalGuid = ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid;
	const FWowEntity* LocalEntity = ConnectionManager->PacketHandler.EntityManager.Find(LocalGuid);
	if (LocalEntity && PlayerBuffBar.IsValid())
	{
		int32 IconsAdded = 0;
		for (int32 i = 0; i < LocalEntity->Auras.Num() && IconsAdded < 16; ++i)
		{
			const FAuraInfo& Aura = LocalEntity->Auras[i];
			if (Aura.bActive && Aura.SpellId > 0)
			{
				// Look up spell information
				const FSpellDbcEntry* SpellEntry = DbcStore.Spells().GetById(Aura.SpellId);
				FString SpellName = TEXT("Unknown");
				FLinearColor AuraColor = FLinearColor::White;

				if (SpellEntry)
				{
					SpellName = SpellEntry->SpellName;
					AuraColor = GetSpellSchoolColor(SpellEntry->SchoolMask);
				}

				// Determine if this is a buff or debuff (simplified - player's own auras are buffs)
				FLinearColor BorderColor = FLinearColor(0.0f, 0.8f, 0.0f); // Green for buffs

				// Create aura icon
				TSharedRef<SWidget> AuraIcon = SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
					.BorderBackgroundColor(AuraColor)
					.Padding(FMargin(1))
					.ToolTipText(FText::FromString(FString::Printf(TEXT("%s%s"),
						*SpellName,
						Aura.Charges > 1 ? *FString::Printf(TEXT(" (%d)"), Aura.Charges) : TEXT(""))))
					[
						SNew(SBox)
						.WidthOverride(18)
						.HeightOverride(18)
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
							.BorderBackgroundColor(BorderColor)
							.Padding(FMargin(0))
							[
								SNew(STextBlock)
								.Text(Aura.Charges > 1 ? FText::FromString(FString::Printf(TEXT("%d"), Aura.Charges)) : FText::GetEmpty())
								.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
								.ColorAndOpacity(FLinearColor::White)
								.Justification(ETextJustify::Center)
							]
						]
					];

				PlayerBuffBar->AddSlot()
				.AutoWidth()
				.Padding(1, 0)
				[
					AuraIcon
				];

				IconsAdded++;
			}
		}
	}

	// Update target auras
	if (TargetGuid != 0)
	{
		const FWowEntity* TargetEntity = ConnectionManager->PacketHandler.EntityManager.Find(TargetGuid);
		if (TargetEntity && TargetBuffBar.IsValid())
		{
			int32 IconsAdded = 0;
			for (int32 i = 0; i < TargetEntity->Auras.Num() && IconsAdded < 16; ++i)
			{
				const FAuraInfo& Aura = TargetEntity->Auras[i];
				if (Aura.bActive && Aura.SpellId > 0)
				{
					// Look up spell information
					const FSpellDbcEntry* SpellEntry = DbcStore.Spells().GetById(Aura.SpellId);
					FString SpellName = TEXT("Unknown");
					FLinearColor AuraColor = FLinearColor::White;

					if (SpellEntry)
					{
						SpellName = SpellEntry->SpellName;
						AuraColor = GetSpellSchoolColor(SpellEntry->SchoolMask);
					}

					// Determine if this is a buff or debuff
					// If caster is the local player, it's a debuff we cast (red border)
					// Otherwise, it's likely a debuff on our target (also red border)
					FLinearColor BorderColor = FLinearColor(0.8f, 0.0f, 0.0f); // Red for debuffs
					if (Aura.CasterGuid == LocalGuid)
					{
						// This is our spell on the target - could be buff or debuff
						// For simplicity, show as green (our beneficial spells)
						BorderColor = FLinearColor(0.0f, 0.8f, 0.0f);
					}

					// Create aura icon
					TSharedRef<SWidget> AuraIcon = SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
						.BorderBackgroundColor(AuraColor)
						.Padding(FMargin(1))
						.ToolTipText(FText::FromString(FString::Printf(TEXT("%s%s"),
							*SpellName,
							Aura.Charges > 1 ? *FString::Printf(TEXT(" (%d)"), Aura.Charges) : TEXT(""))))
						[
							SNew(SBox)
							.WidthOverride(18)
							.HeightOverride(18)
							[
								SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("GenericWhiteBox"))
								.BorderBackgroundColor(BorderColor)
								.Padding(FMargin(0))
								[
									SNew(STextBlock)
									.Text(Aura.Charges > 1 ? FText::FromString(FString::Printf(TEXT("%d"), Aura.Charges)) : FText::GetEmpty())
									.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
									.ColorAndOpacity(FLinearColor::White)
									.Justification(ETextJustify::Center)
								]
							]
						];

					TargetBuffBar->AddSlot()
					.AutoWidth()
					.Padding(1, 0)
					[
						AuraIcon
					];

					IconsAdded++;
				}
			}
		}
	}
}

// ── Guild Roster Methods ──────────────────────────────────────────────────────

void AWowGameplayController::OnGuildRosterKey()
{
	if (!GuildRosterWidget.IsValid())
	{
		CreateGuildRoster();
	}

	if (GuildRosterWidget.IsValid())
	{
		GuildRosterWidget->ToggleVisibility();
	}
}

void AWowGameplayController::CreateGuildRoster()
{
	if (GuildRosterWidget.IsValid())
	{
		return; // Already created
	}

	// Get the packet handler for guild data
	FWowPacketHandler* PacketHandler = nullptr;
	if (ConnectionManager)
	{
		PacketHandler = &ConnectionManager->PacketHandler;
	}

	if (!PacketHandler)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Cannot create guild roster: no packet handler available"));
		return;
	}

	// Create the guild roster widget
	GuildRosterWidget = SNew(SWowGuildRoster, PacketHandler);

	// Add to viewport
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(
			GuildRosterWidget.ToSharedRef(),
			10 // Z-order: above most UI but below tooltips
		);
	}

	// Bind to guild roster updates
	if (PacketHandler)
	{
		PacketHandler->OnGuildRosterUpdated.AddUObject(this, &AWowGameplayController::OnGuildRosterUpdated);
	}
}

void AWowGameplayController::OnGuildRosterUpdated()
{
	if (GuildRosterWidget.IsValid())
	{
		GuildRosterWidget->UpdateGuildRoster();
	}
}

// ── Mailbox Methods ────────────────────────────────────────────────────────────

void AWowGameplayController::ShowMailbox()
{
	if (!MailboxWidget.IsValid())
	{
		CreateMailbox();
	}

	if (MailboxWidget.IsValid())
	{
		MailboxWidget->Show();
	}
}

void AWowGameplayController::CreateMailbox()
{
	if (MailboxWidget.IsValid())
	{
		return; // Already created
	}

	// Get the packet handler for mail data
	FWowPacketHandler* PacketHandler = nullptr;
	if (ConnectionManager)
	{
		PacketHandler = &ConnectionManager->PacketHandler;
	}

	if (!PacketHandler)
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Cannot create mailbox: no packet handler available"));
		return;
	}

	// Create the mailbox widget
	MailboxWidget = SNew(SWowMailbox, PacketHandler);

	// Add to viewport
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(
			MailboxWidget.ToSharedRef(),
			10 // Z-order: above most UI but below tooltips
		);
	}
}

void AWowGameplayController::InitializeAudioSystem(AWowWorldManager* WorldManager)
{
	if (!WorldManager || !GetWorld())
	{
		UE_LOG(LogWowGameplay, Warning, TEXT("Cannot initialize audio system: missing WorldManager or World"));
		return;
	}

	// Check if audio manager already exists
	TArray<AActor*> AudioManagers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWowAudioManager::StaticClass(), AudioManagers);

	AWowAudioManager* AudioManager = nullptr;
	if (AudioManagers.Num() > 0)
	{
		AudioManager = Cast<AWowAudioManager>(AudioManagers[0]);
		UE_LOG(LogWowGameplay, Log, TEXT("Found existing audio manager"));
	}
	else
	{
		// Spawn the audio manager (no fixed name to avoid duplicate name crashes)
		AudioManager = GetWorld()->SpawnActor<AWowAudioManager>(AWowAudioManager::StaticClass());
		if (AudioManager)
		{
			UE_LOG(LogWowGameplay, Log, TEXT("Spawned new audio manager"));
		}
		else
		{
			UE_LOG(LogWowGameplay, Error, TEXT("Failed to spawn audio manager"));
			return;
		}
	}

	// Initialize the audio manager with the MPQ manager
	if (AudioManager && WorldManager->GetMpqManager())
	{
		AudioManager->SetMpqManager(WorldManager->GetMpqManager());

		// Set initial zone if we have a local player
		if (ConnectionManager)
		{
			const FWowEntity* LocalPlayer = ConnectionManager->PacketHandler.EntityManager.GetLocalPlayer();
			if (LocalPlayer)
			{
				// For now, start with a default zone music
				// In a full implementation, you'd get this from the world state or player location
				AudioManager->SetCurrentZone(1, 1); // Dun Morogh as example
			}
		}

		UE_LOG(LogWowGameplay, Log, TEXT("Audio system initialized successfully"));
	}
}

void AWowGameplayController::FireUIEvent(const FString& EventName, const TArray<FString>& Args)
{
	if (UIManager && UIManager->GetEventSystem())
	{
		UIManager->GetEventSystem()->FireEvent(EventName, Args);
	}
}
