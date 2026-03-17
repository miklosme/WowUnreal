#include "WowLoginController.h"
#include "WowConnectionManager.h"
#include "WowLoginWidget.h"
#include "WowRealmSelectWidget.h"
#include "WowCharacterSelectWidget.h"
#include "WowCharacterCreateWidget.h"
#include "WowCinematicManager.h"
#include "WowCharacterPreview.h"
#include "WowCredentialStore.h"
#include "WowGameplayController.h"
#include "WowPlayerCharacter.h"
// removed: now using SWowLoadingScreen.h
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "WowWorldManager.h"
#include "WowAudioManager.h"
#include "WowUIManager.h"
#include "SWowLoadingScreen.h"
#include "SWowCombatLog.h"
#include "SWowChatWindow.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Components/CanvasPanel.h"
#include "Widgets/SOverlay.h"
#include "Formats/Dbc/DbcStore.h"
#include "WowAssetCache.h"
#include "Mpq/MpqManager.h"
#include "WowTextureFactory.h"
#include "Formats/BlpParser.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowLogin, Log, All);

AWowLoginController::AWowLoginController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AWowLoginController::BeginPlay()
{
    Super::BeginPlay();
    StartLoginFlow();
}

void AWowLoginController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearCurrentScreen();
    if (CinematicManager)
    {
        CinematicManager->StopCinematic();
        delete CinematicManager;
        CinematicManager = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

void AWowLoginController::StartLoginFlow()
{
    ConnectionManager = NewObject<UWowConnectionManager>(this);
    ConnectionManager->OnStateChanged.AddDynamic(this, &AWowLoginController::OnStateChanged);
    ConnectionManager->OnRealmList.AddDynamic(this, &AWowLoginController::OnRealmList);
    ConnectionManager->OnCharacterList.AddDynamic(this, &AWowLoginController::OnCharacterList);
    ConnectionManager->OnError.AddDynamic(this, &AWowLoginController::OnError);
    ConnectionManager->OnCharCreateResult.AddDynamic(this, &AWowLoginController::OnCharCreateResult);

    ShowLoginScreen();

    // Show mouse cursor
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->bShowMouseCursor = true;
        PC->SetInputMode(FInputModeGameAndUI());
    }
}

void AWowLoginController::OnStateChanged(EWowSessionState NewState)
{
    UE_LOG(LogWowLogin, Log, TEXT("Session state: %d"), static_cast<int32>(NewState));

    switch (NewState)
    {
    case EWowSessionState::AuthHaveRealmList:
        ShowRealmSelectScreen(ConnectionManager->GetCachedRealms());
        break;
    case EWowSessionState::WorldAuthenticated:
        SetStatusText(TEXT("Requesting character list..."));
        ConnectionManager->RequestCharacterList();
        break;
    case EWowSessionState::WorldHaveCharList:
        // If -createchar flag AND no characters exist, auto-create a Human Mage
        if (FParse::Param(FCommandLine::Get(), TEXT("createchar")) && !bCharCreateSent
            && ConnectionManager->GetCachedCharacters().Num() == 0)
        {
            bCharCreateSent = true;
            FString CharName;
            if (!FParse::Value(FCommandLine::Get(), TEXT("-charname="), CharName))
            {
                // WoW names must be letters only — generate a random valid name
                static const TCHAR* Prefixes[] = { TEXT("Thorn"), TEXT("Ash"), TEXT("Elm"), TEXT("Oak"), TEXT("Frost"), TEXT("Storm"), TEXT("Dawn"), TEXT("Dusk") };
                static const TCHAR* Suffixes[] = { TEXT("wind"), TEXT("blade"), TEXT("fire"), TEXT("song"), TEXT("heart"), TEXT("vale"), TEXT("keep"), TEXT("ward") };
                int32 P = FMath::RandRange(0, UE_ARRAY_COUNT(Prefixes) - 1);
                int32 S = FMath::RandRange(0, UE_ARRAY_COUNT(Suffixes) - 1);
                CharName = FString(Prefixes[P]) + Suffixes[S];
            }
            UE_LOG(LogWowLogin, Log, TEXT("Auto-creating character: %s (Human Mage)"), *CharName);
            // Human=1, Mage=8, Male=0
            ConnectionManager->CreateCharacter(CharName, 1, 8, 0, 0, 0, 0, 0, 0);
            SetStatusText(FString::Printf(TEXT("Creating character %s..."), *CharName));
        }
        else
        {
            ShowCharacterSelectScreen(ConnectionManager->GetCachedCharacters());
        }
        break;
    case EWowSessionState::WorldEnteringWorld:
        // Bind entity events and set defer flag BEFORE the server sends LOGIN_VERIFY_WORLD
        // so we capture the spawn position without teleporting the pawn
        {
            AWowGameplayController* GPC = Cast<AWowGameplayController>(
                UGameplayStatics::GetPlayerController(this, 0));
            if (GPC)
            {
                if (!GPC->ConnectionManager)
                {
                    GPC->ConnectionManager = ConnectionManager;
                    GPC->BindEntityEvents();
                    UE_LOG(LogWowLogin, Log, TEXT("Pre-bound ConnectionManager to GameplayController (before world entry)"));
                }
                // Defer teleport until terrain loads
                GPC->bDeferSpawnTeleport = true;

                // Hide pawn while loading
                if (APawn* Pawn = GPC->GetPawn())
                {
                    Pawn->SetActorHiddenInGame(true);
                    Pawn->SetActorEnableCollision(false);
                    Pawn->SetActorLocation(FVector::ZeroVector);
                    if (ACharacter* Char = Cast<ACharacter>(Pawn))
                    {
                        Char->GetCharacterMovement()->GravityScale = 0.0f;
                        Char->GetCharacterMovement()->Velocity = FVector::ZeroVector;
                    }
                }
            }
        }
        break;
    case EWowSessionState::WorldInGame:
        UE_LOG(LogWowLogin, Log, TEXT("Entered world — showing loading screen"));
        if (CinematicManager)
        {
            CinematicManager->StopCinematic();
            delete CinematicManager;
            CinematicManager = nullptr;
        }
        ClearCurrentScreen();
        ShowLoadingScreen(0);
        InitializeWorldSystems();
        break;
    default:
        break;
    }
}

void AWowLoginController::OnRealmList(const TArray<FWowRealmInfo>& Realms)
{
    if (Realms.Num() == 1)
    {
        UE_LOG(LogWowLogin, Log, TEXT("Auto-selecting single realm: %s"), *Realms[0].Name);
        HandleRealmSelected(0);
    }
}

void AWowLoginController::OnCharacterList(const TArray<FWowCharacterInfo>& Characters)
{
    UE_LOG(LogWowLogin, Log, TEXT("Received %d characters"), Characters.Num());

    // Auto-enter world with first character when -autologin is active
    // Skip if -createchar is set — we'll enter after creation completes
    if (FParse::Param(FCommandLine::Get(), TEXT("autologin")) && Characters.Num() > 0
        && !FParse::Param(FCommandLine::Get(), TEXT("createchar")))
    {
        UE_LOG(LogWowLogin, Log, TEXT("Autologin: entering world with '%s' (GUID %lld)"),
            *Characters[0].Name, Characters[0].Guid);
        HandleCharacterSelected(Characters[0].Guid);
        return;
    }

    // If we just created a character and want to auto-enter
    if (bAutoEnterAfterCreate && Characters.Num() > 0)
    {
        bAutoEnterAfterCreate = false;
        // Enter with the last character (the one just created)
        const FWowCharacterInfo& NewChar = Characters.Last();
        UE_LOG(LogWowLogin, Log, TEXT("Auto-entering world with newly created '%s' (GUID %lld)"),
            *NewChar.Name, NewChar.Guid);
        HandleCharacterSelected(NewChar.Guid);
        return;
    }

    // If we're on the create screen (post-creation refresh), go back to select
    if (CharCreateWidget.IsValid())
    {
        ShowCharacterSelectScreen(Characters);
    }
}

void AWowLoginController::OnError(const FString& Msg)
{
    UE_LOG(LogWowLogin, Error, TEXT("Connection error: %s"), *Msg);
    SetStatusText(FString::Printf(TEXT("Error: %s"), *Msg));
}

void AWowLoginController::OnCharCreateResult(uint8 ResultCode)
{
    if (ResultCode == 0x2F)
    {
        UE_LOG(LogWowLogin, Log, TEXT("Character created successfully"));
        SetStatusText(TEXT("Character created!"));
        // ConnectionManager auto-refreshes char list → OnCharacterList will navigate back

        // If -createchar was used, auto-login with the newly created character
        if (FParse::Param(FCommandLine::Get(), TEXT("createchar")))
        {
            bAutoEnterAfterCreate = true;
        }
    }
    else
    {
        FString ErrMsg;
        switch (ResultCode)
        {
        case 0x30: ErrMsg = TEXT("Name already in use"); break;
        case 0x31: ErrMsg = TEXT("Server disabled"); break;
        case 0x32: ErrMsg = TEXT("Creation failed"); break;
        case 0x33: ErrMsg = TEXT("Disabled"); break;
        case 0x34: ErrMsg = TEXT("Can't create on PvP realm"); break;
        case 0x50: ErrMsg = TEXT("Invalid name"); break;
        case 0x51: ErrMsg = TEXT("Name too short"); break;
        case 0x52: ErrMsg = TEXT("Name too long"); break;
        case 0x53: ErrMsg = TEXT("Name contains invalid characters"); break;
        case 0x54: ErrMsg = TEXT("Name has mixed languages"); break;
        case 0x55: ErrMsg = TEXT("Name is profane"); break;
        case 0x56: ErrMsg = TEXT("Name is reserved"); break;
        case 0x57: ErrMsg = TEXT("Invalid apostrophe"); break;
        case 0x58: ErrMsg = TEXT("Multiple apostrophes"); break;
        case 0x59: ErrMsg = TEXT("Three consecutive letters"); break;
        case 0x5A: ErrMsg = TEXT("Invalid space"); break;
        case 0x5D: ErrMsg = TEXT("Name contains numbers or special chars"); break;
        default: ErrMsg = FString::Printf(TEXT("Creation failed (0x%02X)"), ResultCode); break;
        }
        SetStatusText(FString::Printf(TEXT("Error: %s"), *ErrMsg));
    }
}

void AWowLoginController::InitializeWorldSystems()
{
    UWorld* World = GetWorld();
    if (!World) return;

    // 1. Enable terrain streaming on the WorldManager (loads WDT/WDL)
    if (WorldManager)
    {
        WorldManager->EnableTerrainStreaming();
    }

    // 2. Ensure ConnectionManager is bound to GameplayController
    //    (may already be bound from WorldEnteringWorld state)
    if (AWowGameplayController* GPC = Cast<AWowGameplayController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        if (!GPC->ConnectionManager)
        {
            GPC->ConnectionManager = ConnectionManager;
            GPC->BindEntityEvents();
            UE_LOG(LogWowLogin, Log, TEXT("Bound ConnectionManager to GameplayController"));
        }
    }

    // 3. Start AudioManager
    if (WorldManager && WorldManager->GetMpqManager())
    {
        FActorSpawnParameters AudioParams;
        AudioParams.Name = FName(TEXT("WowAudioManager"));
        AWowAudioManager* AudioMgr = World->SpawnActor<AWowAudioManager>(
            AWowAudioManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, AudioParams);
        if (AudioMgr)
        {
            AudioMgr->SetMpqManager(WorldManager->GetMpqManager());
            UE_LOG(LogWowLogin, Log, TEXT("Spawned AudioManager"));
        }
    }

    // 4. Load UI system (Lua VM + FrameXML)
    if (WorldManager && WorldManager->GetMpqManager())
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UWowUIManager* UIManager = GI->GetSubsystem<UWowUIManager>())
            {
                // Create root canvas for WoW UI frame system
                if (GEngine && GEngine->GameViewport)
                {
                    UCanvasPanel* UIRootCanvas = NewObject<UCanvasPanel>(GetTransientPackage());
                    UIRootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
                    GEngine->GameViewport->AddViewportWidgetContent(
                        UIRootCanvas->TakeWidget(), 50);
                    UIManager->SetRootCanvas(UIRootCanvas);
                }

                UIManager->LoadUI(WorldManager->GetMpqManager());
                UE_LOG(LogWowLogin, Log, TEXT("Loaded WoW UI system"));
            }
        }
    }

    // 5. Create chat window widget
    if (AWowGameplayController* GPC = Cast<AWowGameplayController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        if (GEngine && GEngine->GameViewport)
        {
            // Create the chat window
            GPC->CreateChatWindow();

            if (GPC->ChatWindow.IsValid())
            {
                // Add to viewport at bottom-left with z-order 55 (above UI elements)
                TSharedRef<SWidget> ChatWindowContainer =
                    SNew(SOverlay)
                    .Visibility(EVisibility::SelfHitTestInvisible) // Pass clicks through to 3D world
                    + SOverlay::Slot()
                    .HAlign(HAlign_Left)   // Bottom left positioning
                    .VAlign(VAlign_Bottom)
                    .Padding(20, 0, 0, 100) // 20px from left, 100px from bottom
                    [
                        GPC->ChatWindow.ToSharedRef()
                    ];

                GEngine->GameViewport->AddViewportWidgetContent(ChatWindowContainer, 55);

                UE_LOG(LogWowLogin, Log, TEXT("Created chat window widget"));
            }

            // Still create legacy combat log as backup
            GPC->CombatLog = SNew(SWowCombatLog);
            UE_LOG(LogWowLogin, Log, TEXT("Created backup combat log widget"));
        }
    }
}

void AWowLoginController::HandleLoginSubmit(const FString& Server, int32 Port, const FString& User, const FString& Pass)
{
    UE_LOG(LogWowLogin, Log, TEXT("Login: %s@%s:%d"), *User, *Server, Port);
    SetStatusText(TEXT("Connecting..."));
    ConnectionManager->Login(User, Pass, Server, Port);
}

void AWowLoginController::HandleRealmSelected(int32 Index)
{
    SetStatusText(TEXT("Connecting to realm..."));
    ConnectionManager->SelectRealm(Index);
}

void AWowLoginController::HandleCharacterSelected(int64 Guid)
{
    SetStatusText(TEXT("Entering world..."));

    // Show loading screen (we don't know the map yet, so use 0)
    ShowLoadingScreen(0);

    // Bind to OnLoginVerifyWorld to get the actual map ID for the loading screen
    if (ConnectionManager)
    {
        LoginVerifyWorldHandle = ConnectionManager->PacketHandler.OnLoginVerifyWorld.AddUObject(this, &AWowLoginController::OnLoginVerifyWorld);
    }

    ConnectionManager->EnterWorld(Guid);
}

void AWowLoginController::HandleCreateCharacterRequest()
{
    ShowCharacterCreateScreen();
}

void AWowLoginController::HandleCharacterCreated(const FString& Name, uint8 Race, uint8 Class, uint8 Gender,
    uint8 Skin, uint8 Face, uint8 HairStyle, uint8 HairColor, uint8 FacialHair)
{
    ConnectionManager->CreateCharacter(Name, Race, Class, Gender, Skin, Face, HairStyle, HairColor, FacialHair);
}

void AWowLoginController::HandleBackToCharSelect()
{
    ShowCharacterSelectScreen(ConnectionManager->GetCachedCharacters());
}

void AWowLoginController::HandleCharacterHighlighted(uint8 Race, uint8 Gender)
{
    if (CharacterPreview)
    {
        CharacterPreview->ShowCharacter(Race, Gender);
    }
}

void AWowLoginController::InitializeCinematics()
{
    if (!WorldManager || !WorldManager->GetMpqManager()) return;

    CinematicManager = new FWowCinematicManager();

    // Extract all expansion cinematics
    FMpqManager* Mpq = WorldManager->GetMpqManager();
    CinematicManager->PrepareCinematic(Mpq, EWowExpansion::Classic);
    CinematicManager->PrepareCinematic(Mpq, EWowExpansion::BurningCrusade);
    CinematicManager->PrepareCinematic(Mpq, EWowExpansion::WrathOfTheLichKing);

    // Bind media texture to the login widget
    if (LoginWidget.IsValid())
    {
        LoginWidget->SetCinematicTexture(CinematicManager->GetMediaTexture());
    }

    // Play the cinematic for the current expansion
    EWowExpansion CurrentExp = LoginWidget.IsValid() ? LoginWidget->GetCurrentExpansion() : EWowExpansion::WrathOfTheLichKing;
    if (CinematicManager->HasCinematic(CurrentExp))
    {
        CinematicManager->PlayCinematic(CurrentExp);
        UE_LOG(LogWowLogin, Log, TEXT("Playing login cinematic for expansion %d"), static_cast<int32>(CurrentExp));
    }
}

void AWowLoginController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bWaitingForInitialLoad)
    {
        UpdateLoadingProgress();
    }
}

// ShowLoadingScreen() removed — use ShowLoadingScreen(uint32 MapId) instead

void AWowLoginController::UpdateLoadingProgress()
{
    if (!WorldManager) return;

    AWowGameplayController* GPC = Cast<AWowGameplayController>(
        UGameplayStatics::GetPlayerController(this, 0));

    // Wait for spawn position from server
    if (GPC && GPC->bHasDeferredSpawn && !bHasPendingSpawn)
    {
        bHasPendingSpawn = true;
        PendingSpawnPosition = GPC->DeferredSpawnPos;
        PendingSpawnOrientation = GPC->DeferredSpawnOrientation;

        // Now we know where to spawn — start loading tiles there
        WorldManager->LoadTilesAroundPosition(PendingSpawnPosition, 2);

        LoadingScreenWidget->SetProgress(0.1f, TEXT("Loading terrain..."));
        UE_LOG(LogWowLogin, Log, TEXT("Loading terrain around spawn position"));
    }

    // Update progress
    if (bHasPendingSpawn)
    {
        int32 Queued = WorldManager->GetInitialTilesQueued();
        int32 Loaded = WorldManager->GetInitialTilesLoaded();
        float Progress = (Queued > 0) ? FMath::Clamp((float)Loaded / (float)Queued, 0.0f, 1.0f) : 0.0f;

        FString StatusText = FString::Printf(TEXT("Loading terrain... (%d/%d tiles)"), Loaded, Queued);
        if (LoadingScreenWidget.IsValid())
        {
            LoadingScreenWidget->SetProgress(0.1f + Progress * 0.9f, StatusText);
        }

        if (WorldManager->IsInitialLoadComplete())
        {
            UE_LOG(LogWowLogin, Log, TEXT("Initial terrain loaded — entering world"));
            FinalizeWorldEntry();
        }
    }
}

void AWowLoginController::FinalizeWorldEntry()
{
    bWaitingForInitialLoad = false;
    bHasPendingSpawn = false;

    // Remove loading screen
    if (LoadingWidget.IsValid() && GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(LoadingWidget.ToSharedRef());
        LoadingWidget.Reset();
        LoadingScreenWidget.Reset();
    }

    // Teleport pawn to spawn position and reveal
    AWowGameplayController* GPC = Cast<AWowGameplayController>(
        UGameplayStatics::GetPlayerController(this, 0));
    if (GPC)
    {
        GPC->ApplyDeferredSpawn();
        GPC->bDeferSpawnTeleport = false;

        if (APawn* Pawn = GPC->GetPawn())
        {
            // Snap to ground: trace downward from high above to find terrain surface
            FVector PawnPos = Pawn->GetActorLocation();
            FVector TraceStart = FVector(PawnPos.X, PawnPos.Y, 500000.0f);  // From way above
            FVector TraceEnd = FVector(PawnPos.X, PawnPos.Y, -500000.0f);   // To way below

            FHitResult Hit;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(Pawn);
            QueryParams.bTraceComplex = true; // Use complex collision (mesh geometry)

            UWorld* World = GetWorld();
            if (World && World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, QueryParams))
            {
                // Place pawn's capsule bottom on the ground surface
                // ACharacter origin is at capsule center, so offset by half-height
                float CapsuleHalfHeight = 88.0f;
                if (ACharacter* Char = Cast<ACharacter>(Pawn))
                {
                    CapsuleHalfHeight = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                }
                FVector GroundPos = Hit.ImpactPoint + FVector(0, 0, CapsuleHalfHeight + 2.0f);
                Pawn->SetActorLocation(GroundPos, false, nullptr, ETeleportType::TeleportPhysics);
                UE_LOG(LogWowLogin, Log, TEXT("Snapped to ground: server Z=%.0f → ground Z=%.0f (hit actor: %s)"),
                    PawnPos.Z, GroundPos.Z, *Hit.GetActor()->GetName());
            }
            else
            {
                // No ground found — place 500cm above the server Z as fallback
                FVector FallbackPos = FVector(PawnPos.X, PawnPos.Y, FMath::Max(PawnPos.Z + 500.0f, 5000.0f));
                Pawn->SetActorLocation(FallbackPos, false, nullptr, ETeleportType::TeleportPhysics);
                UE_LOG(LogWowLogin, Warning, TEXT("No ground found at (%.0f, %.0f) — using fallback Z=%.0f"),
                    PawnPos.X, PawnPos.Y, FallbackPos.Z);
            }

            Pawn->SetActorHiddenInGame(false);
            Pawn->SetActorEnableCollision(true);
            if (ACharacter* Char = Cast<ACharacter>(Pawn))
            {
                Char->GetCharacterMovement()->GravityScale = 1.0f;
            }

            UE_LOG(LogWowLogin, Log, TEXT("Player revealed at (%.0f, %.0f, %.0f)"),
                Pawn->GetActorLocation().X, Pawn->GetActorLocation().Y, Pawn->GetActorLocation().Z);
        }

        // WoW-style: cursor always visible, clicks go to both game and UI
        GPC->bShowMouseCursor = true;
        GPC->bEnableClickEvents = true;
        GPC->bEnableMouseOverEvents = true;

        // GameAndUI with NO widget to focus — this keeps mouse free
        FInputModeGameAndUI InputMode;
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        InputMode.SetHideCursorDuringCapture(false);
        GPC->SetInputMode(InputMode);
    }

    UE_LOG(LogWowLogin, Log, TEXT("World entry complete — player is in the world"));
}

void AWowLoginController::HandleExpansionChanged(uint8 ExpansionIndex)
{
    if (!CinematicManager) return;

    EWowExpansion Exp = static_cast<EWowExpansion>(ExpansionIndex);
    if (CinematicManager->HasCinematic(Exp))
    {
        CinematicManager->PlayCinematic(Exp);
        UE_LOG(LogWowLogin, Log, TEXT("Switched cinematic to expansion %d"), ExpansionIndex);
    }
    else
    {
        CinematicManager->StopCinematic();
        UE_LOG(LogWowLogin, Log, TEXT("No cinematic for expansion %d"), ExpansionIndex);
    }
}

void AWowLoginController::ClearCurrentScreen()
{
    if (CurrentWidget.IsValid())
    {
        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->RemoveViewportWidgetContent(CurrentWidget.ToSharedRef());
        }
        CurrentWidget.Reset();
    }
    LoginWidget.Reset();
    RealmSelectWidget.Reset();
    CharSelectWidget.Reset();
    CharCreateWidget.Reset();
    LoadingScreenWidget.Reset();

    // Clean up character preview when leaving character select
    if (CharacterPreview)
    {
        CharacterPreview->ClearCharacter();
        CharacterPreview->Destroy();
        CharacterPreview = nullptr;
    }
}

void AWowLoginController::ShowLoginScreen()
{
    ClearCurrentScreen();

    LoginWidget = SNew(SWowLoginWidget);
    LoginWidget->OnLoginSubmit.BindUObject(this, &AWowLoginController::HandleLoginSubmit);
    LoginWidget->OnExpansionChanged.BindUObject(this, &AWowLoginController::HandleExpansionChanged);
    CurrentWidget = LoginWidget;

    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->AddViewportWidgetContent(CurrentWidget.ToSharedRef(), 100);
    }

    // Prefill from saved credentials
    UWowCredentialStore* CredStore = NewObject<UWowCredentialStore>(GetTransientPackage());
    if (CredStore->LoadCredentials())
    {
        FWowCredential Cred = CredStore->GetDefaultCredential();
        if (!Cred.Username.IsEmpty())
        {
            FString ServerStr = FString::Printf(TEXT("%s:%d"), *Cred.ServerAddress, Cred.AuthPort);
            LoginWidget->SetCredentials(ServerStr, Cred.Username, Cred.Password);
            UE_LOG(LogWowLogin, Log, TEXT("Prefilled credentials for %s@%s"), *Cred.Username, *ServerStr);
        }
    }

    // Initialize cinematics after showing the widget
    InitializeCinematics();

    // Auto-submit if -autologin flag is set
    if (FParse::Param(FCommandLine::Get(), TEXT("autologin")))
    {
        UE_LOG(LogWowLogin, Log, TEXT("Autologin: auto-submitting login form"));
        LoginWidget->AutoSubmit();
    }
}

void AWowLoginController::ShowRealmSelectScreen(const TArray<FWowRealmInfo>& Realms)
{
    ClearCurrentScreen();

    RealmSelectWidget = SNew(SWowRealmSelectWidget);
    RealmSelectWidget->OnRealmSelected.BindUObject(this, &AWowLoginController::HandleRealmSelected);
    RealmSelectWidget->PopulateRealms(Realms);
    CurrentWidget = RealmSelectWidget;

    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->AddViewportWidgetContent(CurrentWidget.ToSharedRef(), 100);
    }
}

void AWowLoginController::ShowCharacterSelectScreen(const TArray<FWowCharacterInfo>& Characters)
{
    ClearCurrentScreen();

    CharSelectWidget = SNew(SWowCharacterSelectWidget);
    CharSelectWidget->OnCharacterSelected.BindUObject(this, &AWowLoginController::HandleCharacterSelected);
    CharSelectWidget->OnCreateCharacterRequest.BindUObject(this, &AWowLoginController::HandleCreateCharacterRequest);
    CharSelectWidget->OnCharacterHighlighted.BindUObject(this, &AWowLoginController::HandleCharacterHighlighted);
    CharSelectWidget->PopulateCharacters(Characters);
    CurrentWidget = CharSelectWidget;

    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->AddViewportWidgetContent(CurrentWidget.ToSharedRef(), 100);
    }

    // Create character preview actor if we have a world manager
    if (WorldManager && !CharacterPreview)
    {
        UWorld* World = GetWorld();
        if (World)
        {
            FActorSpawnParameters PreviewParams;
            PreviewParams.Name = FName(TEXT("WowCharacterPreview"));
            CharacterPreview = World->SpawnActor<AWowCharacterPreview>(
                AWowCharacterPreview::StaticClass(),
                FVector::ZeroVector, FRotator::ZeroRotator, PreviewParams);
            if (CharacterPreview)
            {
                CharacterPreview->Setup(WorldManager);
                CharSelectWidget->SetPreviewRenderTarget(CharacterPreview->GetRenderTarget());
            }
        }
    }

    // Auto-highlight first character
    if (Characters.Num() > 0 && CharacterPreview)
    {
        CharacterPreview->ShowCharacter(Characters[0].Race, Characters[0].Gender);
    }
}

void AWowLoginController::ShowCharacterCreateScreen()
{
    ClearCurrentScreen();

    CharCreateWidget = SNew(SWowCharacterCreateWidget);
    CharCreateWidget->OnCharacterCreated.BindUObject(this, &AWowLoginController::HandleCharacterCreated);
    CharCreateWidget->OnBackToCharSelect.BindUObject(this, &AWowLoginController::HandleBackToCharSelect);
    CurrentWidget = CharCreateWidget;

    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->AddViewportWidgetContent(CurrentWidget.ToSharedRef(), 100);
    }
}

void AWowLoginController::SetStatusText(const FString& Text)
{
    if (LoginWidget.IsValid()) LoginWidget->SetStatusText(Text);
    if (RealmSelectWidget.IsValid()) RealmSelectWidget->SetStatusText(Text);
    if (CharSelectWidget.IsValid()) CharSelectWidget->SetStatusText(Text);
    if (CharCreateWidget.IsValid()) CharCreateWidget->SetStatusText(Text);
    if (LoadingScreenWidget.IsValid()) LoadingScreenWidget->SetProgressText(Text);
}

void AWowLoginController::ShowLoadingScreen(uint32 MapId)
{
    ClearCurrentScreen();

    LoadingScreenWidget = SNew(SWowLoadingScreen);
    CurrentWidget = LoadingScreenWidget;

    // Try to load the appropriate loading screen for this map
    if (WorldManager && WorldManager->GetMpqManager())
    {
        const FDbcStore& DbcStore = FDbcStore::Get();
        UE_LOG(LogWowLogin, Log, TEXT("ShowLoadingScreen: MapId=%u, DbcStore loaded=%s"), MapId, DbcStore.IsLoaded() ? TEXT("true") : TEXT("false"));
        if (DbcStore.IsLoaded())
        {
            // Try to find the loading screen using DBC relationship
            UTexture2D* LoadingTexture = nullptr;
            if (const auto* MapEntry = DbcStore.Maps().GetById(MapId))
            {
                UE_LOG(LogWowLogin, Log, TEXT("Found map entry for MapId=%u, LoadingScreenID=%u"), MapId, MapEntry->LoadingScreenID);
                // Look up the loading screen by ID
                if (const auto* LoadingScreenEntry = DbcStore.LoadingScreens().GetById(MapEntry->LoadingScreenID))
                {
                    FString LoadingScreenPath = LoadingScreenEntry->FileName;
                    UE_LOG(LogWowLogin, Log, TEXT("Found loading screen entry, FileName='%s'"), *LoadingScreenPath);
                    if (!LoadingScreenPath.IsEmpty())
                    {
                        // Ensure proper path format
                        if (!LoadingScreenPath.StartsWith(TEXT("Interface\\")))
                        {
                            LoadingScreenPath = TEXT("Interface\\Glues\\LoadingScreens\\") + LoadingScreenPath;
                        }
                        if (!LoadingScreenPath.EndsWith(TEXT(".blp")))
                        {
                            LoadingScreenPath += TEXT(".blp");
                        }

                        TArray<uint8> BlpData;
                        UE_LOG(LogWowLogin, Log, TEXT("Trying to read loading screen: %s"), *LoadingScreenPath);
                        if (WorldManager->GetMpqManager()->ReadFile(LoadingScreenPath, BlpData))
                        {
                            UE_LOG(LogWowLogin, Log, TEXT("Successfully read BLP file, size=%d bytes"), BlpData.Num());
                            FBlpTexture BlpTex = FBlpParser::Parse(BlpData);
                            if (!BlpTex.MipLevels.IsEmpty())
                            {
                                LoadingTexture = FWowTextureFactory::CreateTexture(BlpTex, LoadingScreenPath);
                                if (LoadingTexture)
                                {
                                    UE_LOG(LogWowLogin, Log, TEXT("Loaded loading screen from DBC: %s"), *LoadingScreenPath);
                                }
                            }
                        }
                    }
                }

                // Fallback: try common loading screen patterns by map name
                if (!LoadingTexture)
                {
                    FString MapName = MapEntry->InternalName;
                    TArray<FString> LoadingScreenPaths = {
                        FString::Printf(TEXT("Interface\\Glues\\LoadingScreens\\%s.blp"), *MapName),
                        FString::Printf(TEXT("Interface\\Glues\\LoadingScreens\\LoadingScreen_%s.blp"), *MapName),
                        FString::Printf(TEXT("Interface\\Glues\\LoadingScreens\\LoadScreen_%s.blp"), *MapName)
                    };

                    for (const FString& Path : LoadingScreenPaths)
                    {
                        TArray<uint8> BlpData;
                        if (WorldManager->GetMpqManager()->ReadFile(Path, BlpData))
                        {
                            FBlpTexture BlpTex = FBlpParser::Parse(BlpData);
                            if (!BlpTex.MipLevels.IsEmpty())
                            {
                                LoadingTexture = FWowTextureFactory::CreateTexture(BlpTex, Path);
                                if (LoadingTexture)
                                {
                                    UE_LOG(LogWowLogin, Log, TEXT("Loaded loading screen by name: %s"), *Path);
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            // Fall back to common loading screens
            if (!LoadingTexture)
            {
                UE_LOG(LogWowLogin, Warning, TEXT("DBC loading screen failed, trying fallback paths"));
                TArray<FString> FallbackPaths = {
                    TEXT("Interface\\Glues\\LoadingScreens\\LoadScreenEasternKingdoms.blp"),
                    TEXT("Interface\\Glues\\LoadingScreens\\LoadScreenKalimdor.blp"),
                    TEXT("Interface\\Glues\\LoadingScreens\\LoadingScreen.blp"),
                    TEXT("Interface\\Glues\\LoadingScreens\\LoadScreen_Barrens.blp"),
                    TEXT("Interface\\Glues\\LoadingScreens\\LoadScreen_Elwynn.blp")
                };

                for (const FString& Path : FallbackPaths)
                {
                    TArray<uint8> BlpData;
                    UE_LOG(LogWowLogin, Log, TEXT("Trying fallback loading screen: %s"), *Path);
                    if (WorldManager->GetMpqManager()->ReadFile(Path, BlpData))
                    {
                        UE_LOG(LogWowLogin, Log, TEXT("Successfully read fallback BLP file, size=%d bytes"), BlpData.Num());
                        FBlpTexture BlpTex = FBlpParser::Parse(BlpData);
                        if (!BlpTex.MipLevels.IsEmpty())
                        {
                            LoadingTexture = FWowTextureFactory::CreateTexture(BlpTex, Path);
                            if (LoadingTexture)
                            {
                                UE_LOG(LogWowLogin, Log, TEXT("Loaded fallback loading screen: %s"), *Path);
                                break;
                            }
                        }
                        else
                        {
                            UE_LOG(LogWowLogin, Warning, TEXT("Failed to parse BLP: %s"), *Path);
                        }
                    }
                    else
                    {
                        UE_LOG(LogWowLogin, Warning, TEXT("Failed to read file: %s"), *Path);
                    }
                }
            }

            if (LoadingTexture)
            {
                LoadingScreenWidget->SetBackgroundImage(LoadingTexture);
                UE_LOG(LogWowLogin, Log, TEXT("Set loading screen background image successfully"));
            }
            else
            {
                UE_LOG(LogWowLogin, Warning, TEXT("No loading screen image could be loaded - showing blank background"));
            }
        }
        else
        {
            UE_LOG(LogWowLogin, Warning, TEXT("DBC Store not loaded - cannot load loading screen image"));
        }
    }
    else
    {
        UE_LOG(LogWowLogin, Warning, TEXT("WorldManager or MpqManager not available - cannot load loading screen image"));
    }

    LoadingScreenWidget->SetProgress(0.0f, TEXT("Loading terrain..."));

    LoadingWidget = LoadingScreenWidget;
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->AddViewportWidgetContent(LoadingWidget.ToSharedRef(), 200);
    }
    bWaitingForInitialLoad = true;
}

void AWowLoginController::OnLoginVerifyWorld(uint32 MapId, float X, float Y, float Z, float Orientation)
{
    // Update loading screen background image if we have one (don't recreate the widget)
    if (LoadingScreenWidget.IsValid() && WorldManager && WorldManager->GetMpqManager() && MapId > 0)
    {
        // Try to load map-specific loading screen image
        // (the widget is already showing from ShowLoadingScreen(0))
        UE_LOG(LogWowLogin, Log, TEXT("LoginVerifyWorld: map=%u, could update loading screen image"), MapId);
    }

    // Unbind the event since we only need it once
    if (ConnectionManager && LoginVerifyWorldHandle.IsValid())
    {
        ConnectionManager->PacketHandler.OnLoginVerifyWorld.Remove(LoginVerifyWorldHandle);
        LoginVerifyWorldHandle.Reset();
    }
}
