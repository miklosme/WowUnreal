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
#include "WowWorldManager.h"
#include "WowAudioManager.h"
#include "WowUIManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Components/CanvasPanel.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowLogin, Log, All);

AWowLoginController::AWowLoginController()
{
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
        ShowCharacterSelectScreen(ConnectionManager->GetCachedCharacters());
        break;
    case EWowSessionState::WorldEnteringWorld:
        // Bind entity events BEFORE the server sends LOGIN_VERIFY_WORLD
        // so we don't miss the spawn position
        {
            AWowGameplayController* GPC = Cast<AWowGameplayController>(
                UGameplayStatics::GetPlayerController(this, 0));
            if (GPC && !GPC->ConnectionManager)
            {
                GPC->ConnectionManager = ConnectionManager;
                GPC->BindEntityEvents();
                UE_LOG(LogWowLogin, Log, TEXT("Pre-bound ConnectionManager to GameplayController (before world entry)"));
            }
        }
        break;
    case EWowSessionState::WorldInGame:
        UE_LOG(LogWowLogin, Log, TEXT("Entered world — removing login UI and initializing world"));
        if (CinematicManager)
        {
            CinematicManager->StopCinematic();
            delete CinematicManager;
            CinematicManager = nullptr;
        }
        ClearCurrentScreen();
        InitializeWorldSystems();
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            PC->bShowMouseCursor = false;
            PC->SetInputMode(FInputModeGameOnly());
        }
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
    if (FParse::Param(FCommandLine::Get(), TEXT("autologin")) && Characters.Num() > 0)
    {
        UE_LOG(LogWowLogin, Log, TEXT("Autologin: entering world with '%s' (GUID %lld)"),
            *Characters[0].Name, Characters[0].Guid);
        HandleCharacterSelected(Characters[0].Guid);
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
    }
    else
    {
        FString ErrMsg;
        switch (ResultCode)
        {
        case 0x30: ErrMsg = TEXT("Name already in use"); break;
        case 0x31: ErrMsg = TEXT("Server disabled"); break;
        case 0x32: ErrMsg = TEXT("Creation failed"); break;
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
}
