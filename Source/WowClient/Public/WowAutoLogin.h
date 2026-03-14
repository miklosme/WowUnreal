#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WowSessionState.h"
#include "WowAutoLogin.generated.h"

class UWowConnectionManager;

UCLASS()
class WOWCLIENT_API UWowAutoLogin : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    /** Whether autologin was requested via command line */
    UPROPERTY(BlueprintReadOnly) bool bAutoLoginRequested = false;

    /** Account alias to use (from -account=) */
    UPROPERTY(BlueprintReadOnly) FString AccountAlias;

    /** Attempt autologin if requested */
    void TryAutoLogin();

    /** Active connection manager created for the auto-login flow, if any */
    UWowConnectionManager* GetConnectionManager() const { return AutoLoginConnMgr; }

private:
    UFUNCTION() void OnRealmListReceived(const TArray<FWowRealmInfo>& Realms);
    UFUNCTION() void OnCharacterListReceived(const TArray<FWowCharacterInfo>& Characters);
    UFUNCTION() void OnAutoLoginError(const FString& Msg);
    UFUNCTION() void OnStateChanged(EWowSessionState NewState);

    UPROPERTY() TObjectPtr<UWowConnectionManager> AutoLoginConnMgr;
};
