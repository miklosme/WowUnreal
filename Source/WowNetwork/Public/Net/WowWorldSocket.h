#pragma once
#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Crypto/WowAuthCrypt.h"

struct FWowCharacterInfo;

DECLARE_DELEGATE_OneParam(FOnWorldAuthResult, bool);
DECLARE_DELEGATE_OneParam(FOnCharacterListReceived, const TArray<FWowCharacterInfo>&);
DECLARE_DELEGATE_TwoParams(FOnWorldPacket, uint16 /*Opcode*/, const TArray<uint8>& /*Data*/);

class WOWNETWORK_API FWowWorldSocket : public FRunnable
{
public:
    FWowWorldSocket();
    virtual ~FWowWorldSocket();

    bool Connect(const FString& Host, int32 Port, const FString& AccountName, const TArray<uint8>& SessionKey, uint32 RealmId);
    void Disconnect();

    /** Send CMSG_CHAR_ENUM to request character list */
    void SendCharEnum();

    /** Send CMSG_PLAYER_LOGIN with character GUID */
    void SendPlayerLogin(uint64 Guid);

    /** Send a raw packet */
    void SendPacket(uint32 Opcode, const TArray<uint8>& Data = {});

    // FRunnable
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;

    FOnWorldAuthResult OnAuthResult;
    FOnCharacterListReceived OnCharacterList;
    FOnWorldPacket OnPacket;

private:
    void HandleAuthChallenge(const TArray<uint8>& Data);
    void HandleAuthResponse(const TArray<uint8>& Data);
    void HandleCharEnum(const TArray<uint8>& Data);

    bool SendRawBytes(const uint8* Data, int32 Len);
    bool RecvExact(uint8* Buffer, int32 Len);

    class FSocket* Socket = nullptr;
    FRunnableThread* Thread = nullptr;

    FWowAuthCrypt Crypt;
    FString AccountName;
    TArray<uint8> SessionKey;
    uint32 RealmId = 1;

    bool bRunning = false;
    bool bEncrypted = false;

    // Thread-safe send queue
    TQueue<TArray<uint8>> SendQueue;

    FCriticalSection SocketLock;
};
