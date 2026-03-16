#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WowSessionState.h"
#include "WowLoginController.generated.h"

class UWowConnectionManager;
class AWowWorldManager;
class AWowAudioManager;
class UWowUIManager;
class SWowLoginWidget;
class SWowRealmSelectWidget;
class SWowCharacterSelectWidget;
class SWowCharacterCreateWidget;

UCLASS()
class WOWUNREAL_API AWowLoginController : public AActor
{
    GENERATED_BODY()
public:
    AWowLoginController();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

    /** Initialize world systems when entering the game world */
    void InitializeWorldSystems();

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
    TSharedPtr<SWidget> CurrentWidget;
};
