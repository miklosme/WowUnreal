#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WowSessionState.h"
#include "WowLoginController.generated.h"

class UWowConnectionManager;
class AWowWorldManager;
class AWowAudioManager;
class UWowUIManager;
class FWowCinematicManager;
class AWowCharacterPreview;
class SWowLoginWidget;
class SWowRealmSelectWidget;
class SWowCharacterSelectWidget;
class SWowCharacterCreateWidget;
class SWowLoadingScreen;

UCLASS()
class WOWUNREAL_API AWowLoginController : public AActor
{
    GENERATED_BODY()
public:
    AWowLoginController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

    void StartLoginFlow();

    UPROPERTY()
    TObjectPtr<UWowConnectionManager> ConnectionManager;

    /** Set by GameMode so we can enable streaming on world entry */
    UPROPERTY()
    TObjectPtr<AWowWorldManager> WorldManager;

private:
    UFUNCTION() void OnStateChanged(EWowSessionState NewState);
    UFUNCTION() void OnRealmList(const TArray<FWowRealmInfo>& Realms);
    UFUNCTION() void OnCharacterList(const TArray<FWowCharacterInfo>& Characters);
    UFUNCTION() void OnError(const FString& Msg);
    UFUNCTION() void OnCharCreateResult(uint8 ResultCode);

    void HandleLoginSubmit(const FString& Server, int32 Port, const FString& User, const FString& Pass);
    void HandleRealmSelected(int32 Index);
    void HandleCharacterSelected(int64 Guid);
    void HandleCreateCharacterRequest();
    void HandleCharacterCreated(const FString& Name, uint8 Race, uint8 Class, uint8 Gender,
        uint8 Skin, uint8 Face, uint8 HairStyle, uint8 HairColor, uint8 FacialHair);
    void HandleBackToCharSelect();
    void HandleExpansionChanged(uint8 ExpansionIndex);
    void HandleCharacterHighlighted(uint8 Race, uint8 Gender);

    /** Initialize world systems when entering the game world */
    void InitializeWorldSystems();

    /** Initialize and start cinematic playback */
    void InitializeCinematics();

    /** Show the loading screen and start terrain loading around spawn */
    void ShowLoadingScreen();

    /** Called each tick while loading — checks progress and transitions to game */
    void UpdateLoadingProgress();

    /** Finalize world entry: teleport pawn, hide loading screen, enable input */
    void FinalizeWorldEntry();

    void ShowLoginScreen();
    void ShowRealmSelectScreen(const TArray<FWowRealmInfo>& Realms);
    void ShowCharacterSelectScreen(const TArray<FWowCharacterInfo>& Characters);
    void ShowCharacterCreateScreen();
    void ClearCurrentScreen();
    void SetStatusText(const FString& Text);

    TSharedPtr<SWowLoginWidget> LoginWidget;
    TSharedPtr<SWowRealmSelectWidget> RealmSelectWidget;
    TSharedPtr<SWowCharacterSelectWidget> CharSelectWidget;
    TSharedPtr<SWowCharacterCreateWidget> CharCreateWidget;
    TSharedPtr<SWowLoadingScreen> LoadingScreenWidget;
    TSharedPtr<SWidget> CurrentWidget;
    TSharedPtr<SWidget> LoadingWidget; // Separate from CurrentWidget so it overlays

    /** Cinematic manager for login screen backgrounds (owned, cleaned up in EndPlay) */
    FWowCinematicManager* CinematicManager = nullptr;

    /** Character preview for 3D model display in character select */
    UPROPERTY()
    TObjectPtr<AWowCharacterPreview> CharacterPreview;

    /** Loading state */
    bool bWaitingForInitialLoad = false;
    FVector PendingSpawnPosition = FVector::ZeroVector;
    float PendingSpawnOrientation = 0.0f;
    bool bHasPendingSpawn = false;
};
