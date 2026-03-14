#pragma once
#include "CoreMinimal.h"
#include "WowSessionState.h"
#include "WowPacketHandler.h"
#include "WowConnectionManager.generated.h"

class FWowAuthSocket;
class FWowWorldSocket;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionStateChanged, EWowSessionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRealmList, const TArray<FWowRealmInfo>&, Realms);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterList, const TArray<FWowCharacterInfo>&, Characters);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWowError, const FString&, Msg);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharCreateResult, uint8, ResultCode);

UCLASS(BlueprintType)
class WOWNETWORK_API UWowConnectionManager : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) void Login(const FString& User, const FString& Pass, const FString& Server, int32 Port = 3724);
    UFUNCTION(BlueprintCallable) void SelectRealm(int32 Index);
    UFUNCTION(BlueprintCallable) void RequestCharacterList();
    UFUNCTION(BlueprintCallable) void EnterWorld(int64 Guid);
    UFUNCTION(BlueprintCallable) void CreateCharacter(const FString& Name, uint8 Race, uint8 Class, uint8 Gender, uint8 Skin = 0, uint8 Face = 0, uint8 HairStyle = 0, uint8 HairColor = 0, uint8 FacialHair = 0);
    UFUNCTION(BlueprintCallable) void DeleteCharacter(int64 Guid);
    UFUNCTION(BlueprintCallable) void Disconnect();
    UFUNCTION(BlueprintCallable) EWowSessionState GetState() const { return State; }

    /** Send a movement packet to the server */
    UFUNCTION(BlueprintCallable) void SendMovement(int32 Opcode, const FVector& Position, float Orientation, int32 MoveFlags);

    /** Send a chat message */
    UFUNCTION(BlueprintCallable) void SendChatMessage(const FString& Message, int32 Type = 1 /*SAY*/, const FString& Language = TEXT(""));

    /** Send keep-alive heartbeat */
    UFUNCTION(BlueprintCallable) void SendKeepAlive();

    /** Set target selection */
    UFUNCTION(BlueprintCallable) void SendSetSelection(int64 TargetGuid);

    /** Cast a spell on the current target (or self if no target) */
    UFUNCTION(BlueprintCallable) void SendCastSpell(int32 SpellId, int64 TargetGuid = 0);

    /** Start melee attack on target */
    UFUNCTION(BlueprintCallable) void SendAttackSwing(int64 TargetGuid);

    /** Stop melee attack */
    UFUNCTION(BlueprintCallable) void SendAttackStop();

    /** Get current target GUID */
    UFUNCTION(BlueprintCallable) int64 GetTargetGuid() const { return TargetGuid; }

    /** Send a raw packet (for advanced use) */
    void SendRawPacket(uint32 Opcode, const TArray<uint8>& Data = {});

    /** Get the cached character list (valid after WorldHaveCharList state) */
    UFUNCTION(BlueprintCallable) TArray<FWowCharacterInfo> GetCachedCharacters() const { return CachedCharacters; }

    /** Get the cached realm list (valid after AuthHaveRealmList state) */
    UFUNCTION(BlueprintCallable) TArray<FWowRealmInfo> GetCachedRealms() const { return CachedRealms; }

    /** Packet handler — dispatches SMSG opcodes and tracks entities */
    FWowPacketHandler PacketHandler;

    UPROPERTY(BlueprintAssignable) FOnSessionStateChanged OnStateChanged;
    UPROPERTY(BlueprintAssignable) FOnRealmList OnRealmList;
    UPROPERTY(BlueprintAssignable) FOnCharacterList OnCharacterList;
    UPROPERTY(BlueprintAssignable) FOnWowError OnError;
    UPROPERTY(BlueprintAssignable) FOnCharCreateResult OnCharCreateResult;
private:
    EWowSessionState State = EWowSessionState::Disconnected;
    void SetState(EWowSessionState S);

    void OnAuthResultReceived(bool bSuccess);
    void OnRealmListReceived(const TArray<FWowRealmInfo>& Realms);
    void OnWorldAuthResult(bool bSuccess);
    void OnWorldCharacterList(const TArray<FWowCharacterInfo>& Characters);

    TSharedPtr<FWowAuthSocket> AuthSocket;
    TSharedPtr<FWowWorldSocket> WorldSocket;
    FString CachedAccountName;
    TArray<uint8> SessionKey;
    TArray<FWowRealmInfo> CachedRealms;
    TArray<FWowCharacterInfo> CachedCharacters;
    int64 TargetGuid = 0;
};
