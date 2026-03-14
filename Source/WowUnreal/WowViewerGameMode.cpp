#include "WowViewerGameMode.h"
#include "WowFlyCamera.h"
#include "WowPlayerCharacter.h"
#include "WowGameplayController.h"
#include "WowDebugHUD.h"
#include "WowAutoLogin.h"
#include "WowWorldManager.h"
#include "WowSkyManager.h"
#include "WowUIManager.h"
#include "WowConnectionManager.h"
#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"
#include "WowCharacterBuilder.h"
#include "WowAudioManager.h"
#include "WowLoginController.h"
#include "WowCredentialStore.h"
#include "WowEntity.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowGameMode, Log, All);

AWowViewerGameMode::AWowViewerGameMode()
{
    // Default to gameplay character pawn — fly camera used only for non-gameplay test scenes
    DefaultPawnClass = AWowPlayerCharacter::StaticClass();
    PlayerControllerClass = AWowGameplayController::StaticClass();
    HUDClass = AWowDebugHUD::StaticClass();
}

void AWowViewerGameMode::SetupPostProcess(UWorld* World)
{
    if (IConsoleVariable* PreExpCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride")))
    {
        PreExpCVar->Set(1.0f);
    }

    APostProcessVolume* PPV = World->SpawnActor<APostProcessVolume>();
    if (PPV)
    {
        PPV->bUnbound = true;
        PPV->Settings.bOverride_AutoExposureMethod = true;
        PPV->Settings.AutoExposureMethod = AEM_Manual;
        PPV->Settings.bOverride_AutoExposureBias = true;
        PPV->Settings.AutoExposureBias = 0.0f;
        PPV->Settings.bOverride_AutoExposureMinBrightness = true;
        PPV->Settings.AutoExposureMinBrightness = 1.0f;
        PPV->Settings.bOverride_AutoExposureMaxBrightness = true;
        PPV->Settings.AutoExposureMaxBrightness = 1.0f;
    }
}

AWowWorldManager* AWowViewerGameMode::SpawnWorldManager(UWorld* World)
{
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(World, AWowWorldManager::StaticClass(), Found);

    if (Found.Num() > 0)
    {
        return Cast<AWowWorldManager>(Found[0]);
    }

    FActorSpawnParameters Params;
    Params.Name = FName(TEXT("WowWorldManager"));
    AWowWorldManager* WM = World->SpawnActor<AWowWorldManager>(
        AWowWorldManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (WM)
    {
        UE_LOG(LogWowGameMode, Log, TEXT("Spawned WowWorldManager"));
    }
    return WM;
}

void AWowViewerGameMode::SpawnGroundPlane(UWorld* World, const FVector& Center, float Size)
{
    // Spawn a flat static mesh plane for test scenes
    UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (!PlaneMesh) return;

    AStaticMeshActor* PlaneActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(FRotator::ZeroRotator, Center));
    if (!PlaneActor) return;

    PlaneActor->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
    PlaneActor->SetActorScale3D(FVector(Size, Size, 1.0f));
    PlaneActor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PlaneActor->GetStaticMeshComponent()->SetCollisionObjectType(ECC_WorldStatic);
    PlaneActor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);

    UE_LOG(LogWowGameMode, Log, TEXT("Spawned ground plane at %s (scale %.0f)"), *Center.ToString(), Size);
}

void AWowViewerGameMode::SpawnDirectionalLight(UWorld* World)
{
    ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(
        ADirectionalLight::StaticClass(), FTransform(FRotator(-45.0f, 30.0f, 0.0f), FVector::ZeroVector));
    if (Sun)
    {
        Sun->GetLightComponent()->SetIntensity(3.14159f);
        Sun->GetLightComponent()->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f));
    }
}

void AWowViewerGameMode::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (!World) return;

    // Parse test scene selector
    FString TestScene;
    FParse::Value(FCommandLine::Get(), TEXT("-testscene="), TestScene);
    TestScene = TestScene.ToLower();

    // Post-process is always needed
    SetupPostProcess(World);

    if (TestScene == TEXT("character"))
    {
        SetupCharacterTestScene(World);
    }
    else if (TestScene == TEXT("terrain"))
    {
        SetupTerrainTestScene(World);
    }
    else if (TestScene == TEXT("wmo"))
    {
        SetupWmoTestScene(World);
    }
    else if (TestScene == TEXT("ui"))
    {
        SetupUITestScene(World);
    }
    else if (TestScene == TEXT("login"))
    {
        SetupLoginScene(World);
    }
    else if (TestScene == TEXT("network"))
    {
        SetupNetworkTestScene(World);
    }
    else
    {
        // Default: full world with auto-login, or login screen if no -autologin flag
        bool bAutoLogin = FParse::Param(FCommandLine::Get(), TEXT("autologin"));
        if (bAutoLogin)
        {
            SetupDefaultScene(World);
        }
        else
        {
            SetupLoginScene(World);
        }
    }
}

void AWowViewerGameMode::SetupDefaultScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("Starting default (full world) scene"));

    // Spawn sky
    {
        FActorSpawnParameters SkyParams;
        SkyParams.Name = FName(TEXT("WowSkyManager"));
        World->SpawnActor<AWowSkyManager>(AWowSkyManager::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SkyParams);
    }

    // World manager (full streaming)
    AWowWorldManager* WorldManager = SpawnWorldManager(World);

    // Spawn audio manager
    if (WorldManager && WorldManager->GetMpqManager())
    {
        FActorSpawnParameters AudioParams;
        AudioParams.Name = FName(TEXT("WowAudioManager"));
        AWowAudioManager* AudioMgr = World->SpawnActor<AWowAudioManager>(
            AWowAudioManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, AudioParams);
        if (AudioMgr)
        {
            AudioMgr->SetMpqManager(WorldManager->GetMpqManager());
        }
    }

    // Load UI
    if (WorldManager && WorldManager->GetMpqManager())
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UWowUIManager* UIManager = GI->GetSubsystem<UWowUIManager>())
            {
                UIManager->LoadUI(WorldManager->GetMpqManager());
            }
        }
    }

    // Auto-login + gameplay controller binding
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UWowAutoLogin* AutoLogin = GI->GetSubsystem<UWowAutoLogin>())
        {
            AutoLogin->TryAutoLogin();
            if (AWowGameplayController* GPC = Cast<AWowGameplayController>(UGameplayStatics::GetPlayerController(this, 0)))
            {
                if (UWowConnectionManager* CM = AutoLogin->GetConnectionManager())
                {
                    GPC->ConnectionManager = CM;
                    GPC->BindEntityEvents();
                }
            }
        }
    }
}

void AWowViewerGameMode::SetupCharacterTestScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("Starting character/animation test scene"));

    SpawnDirectionalLight(World);
    SpawnGroundPlane(World, FVector::ZeroVector, 100.0f);

    // Teleport player just above the ground plane
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC && PC->GetPawn())
    {
        PC->GetPawn()->SetActorLocation(FVector(0.0f, 0.0f, 200.0f));
        PC->SetControlRotation(FRotator(-15.0f, 0.0f, 0.0f));
    }

    // Spawn world manager for MPQ access (terrain loading skipped via -testscene check in WM::BeginPlay)
    AWowWorldManager* WM = SpawnWorldManager(World);

    // Spawn test character models — Human Male and Female for visual verification
    if (WM && WM->GetMpqManager() && WM->GetAssetCache())
    {
        FWowCharacterBuilder::SpawnCharacter(World, WM->GetMpqManager(), WM->GetAssetCache(),
            FWowCharacterBuilder::ERace::Human, FWowCharacterBuilder::EGender::Male,
            FVector(0.0f, 0.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));

        FWowCharacterBuilder::SpawnCharacter(World, WM->GetMpqManager(), WM->GetAssetCache(),
            FWowCharacterBuilder::ERace::Human, FWowCharacterBuilder::EGender::Female,
            FVector(0.0f, 300.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));

        // Spawn an Orc for variety
        FWowCharacterBuilder::SpawnCharacter(World, WM->GetMpqManager(), WM->GetAssetCache(),
            FWowCharacterBuilder::ERace::Orc, FWowCharacterBuilder::EGender::Male,
            FVector(0.0f, -300.0f, 0.0f), FRotator(0.0f, 180.0f, 0.0f));
    }
}

void AWowViewerGameMode::SetupTerrainTestScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("Starting single-tile terrain test scene"));

    // Spawn sky for proper lighting
    {
        FActorSpawnParameters SkyParams;
        SkyParams.Name = FName(TEXT("WowSkyManager"));
        World->SpawnActor<AWowSkyManager>(AWowSkyManager::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SkyParams);
    }

    // Spawn world manager — loads 3x3 grid then streaming is disabled via -testscene check
    SpawnWorldManager(World);
}

void AWowViewerGameMode::SetupWmoTestScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("Starting WMO test scene"));

    SpawnDirectionalLight(World);

    // Spawn world manager — loads 3x3 grid with WMOs, streaming disabled via -testscene check
    SpawnWorldManager(World);
}

void AWowViewerGameMode::SetupUITestScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("Starting UI test scene"));

    SpawnDirectionalLight(World);
    SpawnGroundPlane(World, FVector::ZeroVector, 10.0f);

    // Teleport player
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC && PC->GetPawn())
    {
        PC->GetPawn()->SetActorLocation(FVector(0.0f, 0.0f, 200.0f));
    }

    // Spawn world manager for MPQ access (terrain loading skipped via -testscene check)
    AWowWorldManager* WM = SpawnWorldManager(World);

    // Load UI
    if (WM && WM->GetMpqManager())
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UWowUIManager* UIManager = GI->GetSubsystem<UWowUIManager>())
            {
                UIManager->LoadUI(WM->GetMpqManager());
            }
        }
    }
}

void AWowViewerGameMode::SetupLoginScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("Starting login screen scene"));

    // Spawn sky for background
    {
        FActorSpawnParameters SkyParams;
        SkyParams.Name = FName(TEXT("WowSkyManager"));
        World->SpawnActor<AWowSkyManager>(AWowSkyManager::StaticClass(),
            FVector::ZeroVector, FRotator::ZeroRotator, SkyParams);
    }

    SpawnDirectionalLight(World);

    // No world manager needed for login — just UI over sky background

    // Spawn the login flow controller (shows login screen UI)
    FActorSpawnParameters LoginParams;
    LoginParams.Name = FName(TEXT("WowLoginController"));
    World->SpawnActor<AWowLoginController>(AWowLoginController::StaticClass(),
        FVector::ZeroVector, FRotator::ZeroRotator, LoginParams);
}

void AWowViewerGameMode::SetupNetworkTestScene(UWorld* World)
{
    UE_LOG(LogWowGameMode, Log, TEXT("=== NETWORK TEST SCENE ==="));
    UE_LOG(LogWowGameMode, Log, TEXT("Headless networking test — no rendering, connects to test server"));
    UE_LOG(LogWowGameMode, Log, TEXT("Tests: auth handshake, realm list, character enum, world login, packet handling, entity updates"));

    // Parse optional timeout from command line (default 30s)
    float Timeout = 30.0f;
    FParse::Value(FCommandLine::Get(), TEXT("-nettesttimeout="), Timeout);
    NetworkTestTimeout = FMath::Clamp(Timeout, 5.0f, 120.0f);

    // Load credentials
    UWowCredentialStore* CredStore = NewObject<UWowCredentialStore>(GetTransientPackage());
    if (!CredStore->LoadCredentials())
    {
        UE_LOG(LogWowGameMode, Error, TEXT("[NETTEST] FAIL: Could not load credentials"));
        FGenericPlatformMisc::RequestExit(false);
        return;
    }

    FString AccountAlias;
    FParse::Value(FCommandLine::Get(), TEXT("-account="), AccountAlias);

    FWowCredential Cred;
    if (AccountAlias.IsEmpty())
    {
        Cred = CredStore->GetDefaultCredential();
    }
    else
    {
        bool bFound = false;
        for (const FWowCredential& C : CredStore->GetAllCredentials())
        {
            if (C.Alias.Equals(AccountAlias, ESearchCase::IgnoreCase))
            {
                Cred = C;
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            UE_LOG(LogWowGameMode, Error, TEXT("[NETTEST] FAIL: Credential alias '%s' not found"), *AccountAlias);
            FGenericPlatformMisc::RequestExit(false);
            return;
        }
    }

    if (Cred.Username.IsEmpty() || Cred.Password.IsEmpty())
    {
        UE_LOG(LogWowGameMode, Error, TEXT("[NETTEST] FAIL: Credential has empty username or password"));
        FGenericPlatformMisc::RequestExit(false);
        return;
    }

    UE_LOG(LogWowGameMode, Log, TEXT("[NETTEST] Connecting as '%s' to %s:%d (timeout %.0fs)"),
           *Cred.Username, *Cred.ServerAddress, Cred.AuthPort, NetworkTestTimeout);

    // Create connection manager
    NetworkTestConnMgr = NewObject<UWowConnectionManager>(GetGameInstance());
    NetworkTestConnMgr->AddToRoot();

    // Bind events
    NetworkTestConnMgr->OnStateChanged.AddDynamic(this, &AWowViewerGameMode::OnNetworkTestStateChanged);
    NetworkTestConnMgr->OnError.AddDynamic(this, &AWowViewerGameMode::OnNetworkTestError);
    NetworkTestConnMgr->OnRealmList.AddDynamic(this, &AWowViewerGameMode::OnNetworkTestRealmList);
    NetworkTestConnMgr->OnCharacterList.AddDynamic(this, &AWowViewerGameMode::OnNetworkTestCharacterList);

    // Start login
    NetworkTestConnMgr->Login(Cred.Username, Cred.Password, Cred.ServerAddress, Cred.AuthPort);

    // Set up a periodic tick timer (every 1s) to log status and check timeout
    NetworkTestElapsed = 0.0f;
    bNetworkTestComplete = false;
    FTimerDelegate TimerDel;
    TimerDel.BindWeakLambda(this, [this]() { NetworkTestTick(); });
    World->GetTimerManager().SetTimer(NetworkTestTimerHandle, TimerDel, 1.0f, true, 1.0f);
}

void AWowViewerGameMode::OnNetworkTestStateChanged(EWowSessionState NewState)
{
    static const TCHAR* StateNames[] = {
        TEXT("Disconnected"),
        TEXT("AuthConnecting"), TEXT("AuthChallengeSent"), TEXT("AuthProofSent"),
        TEXT("AuthRequestingRealmList"), TEXT("AuthHaveRealmList"),
        TEXT("WorldConnecting"), TEXT("WorldAuthChallengeReceived"), TEXT("WorldAuthSessionSent"),
        TEXT("WorldAuthenticated"), TEXT("WorldRequestingCharList"), TEXT("WorldHaveCharList"),
        TEXT("WorldEnteringWorld"), TEXT("WorldInGame"),
        TEXT("Error")
    };
    int32 Idx = static_cast<int32>(NewState);
    const TCHAR* Name = (Idx >= 0 && Idx < UE_ARRAY_COUNT(StateNames)) ? StateNames[Idx] : TEXT("Unknown");
    UE_LOG(LogWowGameMode, Log, TEXT("[NETTEST] State: %s"), Name);

    if (!NetworkTestConnMgr) return;

    // Auto-progress through the login flow
    if (NewState == EWowSessionState::WorldAuthenticated)
    {
        UE_LOG(LogWowGameMode, Log, TEXT("[NETTEST] PASS: World auth complete, requesting character list"));
        NetworkTestConnMgr->RequestCharacterList();
    }
}

void AWowViewerGameMode::OnNetworkTestError(const FString& Msg)
{
    UE_LOG(LogWowGameMode, Error, TEXT("[NETTEST] ERROR: %s"), *Msg);
}

void AWowViewerGameMode::OnNetworkTestRealmList(const TArray<FWowRealmInfo>& Realms)
{
    UE_LOG(LogWowGameMode, Log, TEXT("[NETTEST] PASS: Received %d realm(s)"), Realms.Num());
    for (int32 i = 0; i < Realms.Num(); ++i)
    {
        UE_LOG(LogWowGameMode, Log, TEXT("[NETTEST]   Realm %d: '%s' (%s:%d, %d chars)"),
               i, *Realms[i].Name, *Realms[i].Address, Realms[i].Port, Realms[i].CharacterCount);
    }

    if (NetworkTestConnMgr && Realms.Num() > 0)
    {
        UE_LOG(LogWowGameMode, Log, TEXT("[NETTEST] Selecting realm 0"));
        NetworkTestConnMgr->SelectRealm(0);
    }
}

void AWowViewerGameMode::OnNetworkTestCharacterList(const TArray<FWowCharacterInfo>& Characters)
{
    UE_LOG(LogWowGameMode, Log, TEXT("[NETTEST] PASS: Received %d character(s)"), Characters.Num());
    for (int32 i = 0; i < Characters.Num(); ++i)
    {
        const FWowCharacterInfo& C = Characters[i];
        UE_LOG(LogWowGameMode, Log, TEXT("[NETTEST]   Char %d: '%s' Level %d (Race %d, Class %d, Map %d, Zone %d)"),
               i, *C.Name, C.Level, C.Race, C.Class, C.MapId, C.ZoneId);
    }

    if (NetworkTestConnMgr && Characters.Num() > 0)
    {
        UE_LOG(LogWowGameMode, Log, TEXT("[NETTEST] Entering world with '%s' (GUID %llu)"),
               *Characters[0].Name, Characters[0].Guid);
        NetworkTestConnMgr->EnterWorld(Characters[0].Guid);
    }
}

void AWowViewerGameMode::NetworkTestTick()
{
    NetworkTestElapsed += 1.0f;

    if (!NetworkTestConnMgr || bNetworkTestComplete)
    {
        return;
    }

    EWowSessionState CurrentState = NetworkTestConnMgr->GetState();
    FWowEntityManager& EM = NetworkTestConnMgr->PacketHandler.EntityManager;
    const TSet<uint32>& Spells = NetworkTestConnMgr->PacketHandler.KnownSpells;

    // Log periodic status while in-game
    if (CurrentState == EWowSessionState::WorldInGame)
    {
        int32 PlayerCount = 0;
        int32 UnitCount = 0;
        int32 GOCount = 0;
        for (const auto& Pair : EM.GetAll())
        {
            if (Pair.Value->IsPlayer()) ++PlayerCount;
            else if (Pair.Value->IsUnit()) ++UnitCount;
            else if (Pair.Value->IsGameObject()) ++GOCount;
        }

        UE_LOG(LogWowGameMode, Log, TEXT("[NETTEST] %.0fs — InGame | Entities: %d total (%d players, %d units, %d GOs) | Spells: %d"),
               NetworkTestElapsed, EM.Num(), PlayerCount, UnitCount, GOCount, Spells.Num());

        // Check local player
        if (FWowEntity* LocalPlayer = EM.GetLocalPlayer())
        {
            UE_LOG(LogWowGameMode, Log, TEXT("[NETTEST]   LocalPlayer GUID=%llu Pos=(%.1f, %.1f, %.1f) HP=%d/%d Level=%d"),
                   LocalPlayer->Guid,
                   LocalPlayer->Movement.Position.X, LocalPlayer->Movement.Position.Y, LocalPlayer->Movement.Position.Z,
                   LocalPlayer->GetHealth(), LocalPlayer->GetMaxHealth(), LocalPlayer->GetLevel());
        }
    }

    // After 10 seconds in-game (or on timeout), print final summary and exit
    bool bInGameLongEnough = (CurrentState == EWowSessionState::WorldInGame && NetworkTestElapsed >= 15.0f);
    bool bTimedOut = (NetworkTestElapsed >= NetworkTestTimeout);

    if (bInGameLongEnough || bTimedOut)
    {
        bNetworkTestComplete = true;

        UE_LOG(LogWowGameMode, Log, TEXT(""));
        UE_LOG(LogWowGameMode, Log, TEXT("=== NETWORK TEST RESULTS ==="));
        UE_LOG(LogWowGameMode, Log, TEXT("  Duration:     %.0f seconds"), NetworkTestElapsed);
        UE_LOG(LogWowGameMode, Log, TEXT("  Final State:  %d (%s)"),
               static_cast<int32>(CurrentState),
               CurrentState == EWowSessionState::WorldInGame ? TEXT("WorldInGame") : TEXT("NOT InGame"));

        bool bAuthOk = static_cast<int32>(CurrentState) >= static_cast<int32>(EWowSessionState::AuthHaveRealmList);
        bool bWorldOk = static_cast<int32>(CurrentState) >= static_cast<int32>(EWowSessionState::WorldAuthenticated);
        bool bCharOk = static_cast<int32>(CurrentState) >= static_cast<int32>(EWowSessionState::WorldHaveCharList);
        bool bInGame = CurrentState == EWowSessionState::WorldInGame;
        bool bEntities = EM.Num() > 0;
        bool bLocalPlayer = EM.GetLocalPlayer() != nullptr;

        UE_LOG(LogWowGameMode, Log, TEXT("  Auth:         %s"), bAuthOk ? TEXT("PASS") : TEXT("FAIL"));
        UE_LOG(LogWowGameMode, Log, TEXT("  World Auth:   %s"), bWorldOk ? TEXT("PASS") : TEXT("FAIL"));
        UE_LOG(LogWowGameMode, Log, TEXT("  Char Enum:    %s"), bCharOk ? TEXT("PASS") : TEXT("FAIL"));
        UE_LOG(LogWowGameMode, Log, TEXT("  In Game:      %s"), bInGame ? TEXT("PASS") : TEXT("FAIL"));
        UE_LOG(LogWowGameMode, Log, TEXT("  Entities:     %s (%d tracked)"), bEntities ? TEXT("PASS") : TEXT("FAIL"), EM.Num());
        UE_LOG(LogWowGameMode, Log, TEXT("  Local Player: %s (GUID %llu)"), bLocalPlayer ? TEXT("PASS") : TEXT("FAIL"), EM.LocalPlayerGuid);
        UE_LOG(LogWowGameMode, Log, TEXT("  Known Spells: %d"), Spells.Num());

        bool bAllPass = bAuthOk && bWorldOk && bCharOk && bInGame && bEntities && bLocalPlayer;
        UE_LOG(LogWowGameMode, Log, TEXT("  OVERALL:      %s"), bAllPass ? TEXT("ALL PASS") : TEXT("SOME FAILED"));
        UE_LOG(LogWowGameMode, Log, TEXT("============================="));
        UE_LOG(LogWowGameMode, Log, TEXT(""));

        // Cleanup and exit
        if (NetworkTestConnMgr)
        {
            NetworkTestConnMgr->Disconnect();
            NetworkTestConnMgr->RemoveFromRoot();
            NetworkTestConnMgr = nullptr;
        }

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(NetworkTestTimerHandle);
        }

        FGenericPlatformMisc::RequestExit(false);
    }
}
