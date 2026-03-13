#pragma once
#include "CoreMinimal.h"
#include "WowSessionState.h"
#include "WowConnectionManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionStateChanged, EWowSessionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRealmList, const TArray<FWowRealmInfo>&, Realms);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterList, const TArray<FWowCharacterInfo>&, Characters);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWowError, const FString&, Msg);

UCLASS(BlueprintType)
class WOWNETWORK_API UWowConnectionManager : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) void Login(const FString& User, const FString& Pass, const FString& Server, int32 Port = 3724);
    UFUNCTION(BlueprintCallable) void SelectRealm(int32 Index);
    UFUNCTION(BlueprintCallable) void RequestCharacterList();
    UFUNCTION(BlueprintCallable) void EnterWorld(uint64 Guid);
    UFUNCTION(BlueprintCallable) void Disconnect();
    UFUNCTION(BlueprintCallable) EWowSessionState GetState() const { return State; }

    UPROPERTY(BlueprintAssignable) FOnSessionStateChanged OnStateChanged;
    UPROPERTY(BlueprintAssignable) FOnRealmList OnRealmList;
    UPROPERTY(BlueprintAssignable) FOnCharacterList OnCharacterList;
    UPROPERTY(BlueprintAssignable) FOnWowError OnError;
private:
    EWowSessionState State = EWowSessionState::Disconnected;
    void SetState(EWowSessionState S);
};
