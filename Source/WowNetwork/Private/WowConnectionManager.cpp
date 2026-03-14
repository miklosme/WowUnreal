#include "WowConnectionManager.h"
#include "Net/WowAuthSocket.h"
#include "Net/WowWorldSocket.h"
#include "WowOpcodes.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowNet, Log, All);

void UWowConnectionManager::SetState(EWowSessionState S)
{
    State = S;
    OnStateChanged.Broadcast(S);
}

void UWowConnectionManager::Login(const FString& U, const FString& P, const FString& S, int32 Port)
{
    UE_LOG(LogWowNet, Log, TEXT("Login: %s@%s:%d"), *U, *S, Port);

    // Disconnect any existing session
    Disconnect();

    CachedAccountName = U;
    SetState(EWowSessionState::AuthConnecting);

    AuthSocket = MakeShared<FWowAuthSocket>();
    AuthSocket->OnAuthResult.BindUObject(this, &UWowConnectionManager::OnAuthResultReceived);
    AuthSocket->OnRealmList.BindUObject(this, &UWowConnectionManager::OnRealmListReceived);

    if (!AuthSocket->Connect(S, Port, U, P))
    {
        UE_LOG(LogWowNet, Error, TEXT("Failed to connect to auth server"));
        SetState(EWowSessionState::Error);
        OnError.Broadcast(TEXT("Failed to connect to auth server"));
        AuthSocket.Reset();
        return;
    }

    SetState(EWowSessionState::AuthChallengeSent);
}

void UWowConnectionManager::OnAuthResultReceived(bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogWowNet, Log, TEXT("Authentication successful"));
        SessionKey = AuthSocket->GetSessionKey();
        SetState(EWowSessionState::AuthProofSent);

        // Automatically request realm list
        SetState(EWowSessionState::AuthRequestingRealmList);
        AuthSocket->RequestRealmList();
    }
    else
    {
        UE_LOG(LogWowNet, Error, TEXT("Authentication failed"));
        SetState(EWowSessionState::Error);
        OnError.Broadcast(TEXT("Authentication failed"));
        AuthSocket.Reset();
    }
}

void UWowConnectionManager::OnRealmListReceived(const TArray<FWowRealmInfo>& Realms)
{
    UE_LOG(LogWowNet, Log, TEXT("Received %d realms"), Realms.Num());
    CachedRealms = Realms;
    SetState(EWowSessionState::AuthHaveRealmList);
    OnRealmList.Broadcast(Realms);
}

void UWowConnectionManager::SelectRealm(int32 I)
{
    UE_LOG(LogWowNet, Log, TEXT("Realm: %d"), I);

    if (!CachedRealms.IsValidIndex(I))
    {
        UE_LOG(LogWowNet, Error, TEXT("Invalid realm index: %d"), I);
        OnError.Broadcast(TEXT("Invalid realm index"));
        return;
    }

    const FWowRealmInfo& Realm = CachedRealms[I];
    UE_LOG(LogWowNet, Log, TEXT("Selected realm: %s id=%u (%s:%d)"), *Realm.Name, Realm.RealmId, *Realm.Address, Realm.Port);

    if (SessionKey.Num() == 0)
    {
        UE_LOG(LogWowNet, Error, TEXT("Cannot connect to world server: no session key (auth not completed)"));
        OnError.Broadcast(TEXT("No session key — authenticate first"));
        return;
    }

    SetState(EWowSessionState::WorldConnecting);

    // Disconnect auth socket -- no longer needed
    if (AuthSocket.IsValid())
    {
        AuthSocket->Disconnect();
        AuthSocket.Reset();
    }

    // Create and connect world socket
    WorldSocket = MakeShared<FWowWorldSocket>();
    WorldSocket->OnAuthResult.BindUObject(this, &UWowConnectionManager::OnWorldAuthResult);
    WorldSocket->OnCharacterList.BindUObject(this, &UWowConnectionManager::OnWorldCharacterList);

    // Route unhandled server packets through the packet handler (fires on game thread)
    WorldSocket->OnPacket.BindLambda([this](uint16 Opcode, const TArray<uint8>& Data)
    {
        // Intercept char create/delete results
        if (Opcode == WowOpcode::SMSG_CHAR_CREATE || Opcode == WowOpcode::SMSG_CHAR_DELETE)
        {
            uint8 Result = Data.Num() > 0 ? Data[0] : 0xFF;
            OnCharCreateResult.Broadcast(Result);
            // Auto-refresh character list on success (0x2F = CHAR_CREATE_SUCCESS, 0x3A = CHAR_DELETE_SUCCESS)
            if (Result == 0x2F || Result == 0x3A)
            {
                RequestCharacterList();
            }
            return;
        }
        PacketHandler.HandlePacket(Opcode, Data);
    });

    // Allow packet handler to send responses back to the server (e.g. TIME_SYNC_RESP)
    PacketHandler.OnSendPacket.BindLambda([this](uint32 Opcode, const TArray<uint8>& Data)
    {
        if (WorldSocket.IsValid())
        {
            WorldSocket->SendPacket(Opcode, Data);
        }
    });

    if (!WorldSocket->Connect(Realm.Address, Realm.Port, CachedAccountName, SessionKey, Realm.RealmId))
    {
        UE_LOG(LogWowNet, Error, TEXT("Failed to connect to world server %s:%d"), *Realm.Address, Realm.Port);
        SetState(EWowSessionState::Error);
        OnError.Broadcast(TEXT("Failed to connect to world server"));
        WorldSocket.Reset();
        return;
    }

    UE_LOG(LogWowNet, Log, TEXT("Connecting to world server %s:%d ..."), *Realm.Address, Realm.Port);
}

void UWowConnectionManager::OnWorldAuthResult(bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogWowNet, Log, TEXT("World server authenticated"));
        SetState(EWowSessionState::WorldAuthenticated);
    }
    else
    {
        UE_LOG(LogWowNet, Error, TEXT("World server authentication failed"));
        SetState(EWowSessionState::Error);
        OnError.Broadcast(TEXT("World server authentication failed"));
        WorldSocket.Reset();
    }
}

void UWowConnectionManager::OnWorldCharacterList(const TArray<FWowCharacterInfo>& Characters)
{
    UE_LOG(LogWowNet, Log, TEXT("Received %d characters from world server"), Characters.Num());
    CachedCharacters = Characters;
    SetState(EWowSessionState::WorldHaveCharList);
    OnCharacterList.Broadcast(Characters);
}

void UWowConnectionManager::RequestCharacterList()
{
    UE_LOG(LogWowNet, Log, TEXT("Requesting character list"));

    if (!WorldSocket.IsValid())
    {
        UE_LOG(LogWowNet, Error, TEXT("No world socket connection"));
        OnError.Broadcast(TEXT("Not connected to world server"));
        return;
    }

    SetState(EWowSessionState::WorldRequestingCharList);
    WorldSocket->SendCharEnum();
}

void UWowConnectionManager::EnterWorld(int64 G)
{
    UE_LOG(LogWowNet, Log, TEXT("Enter world: GUID %llu"), G);

    if (!WorldSocket.IsValid())
    {
        UE_LOG(LogWowNet, Error, TEXT("No world socket connection"));
        OnError.Broadcast(TEXT("Not connected to world server"));
        return;
    }

    PacketHandler.EntityManager.LocalPlayerGuid = static_cast<uint64>(G);

    // Listen for LOGIN_VERIFY_WORLD to transition to InGame state
    PacketHandler.OnLoginVerifyWorld.AddLambda([this](uint32 MapId, float X, float Y, float Z, float Orientation)
    {
        UE_LOG(LogWowNet, Log, TEXT("Entered world: map=%d pos=(%.1f, %.1f, %.1f) orient=%.2f"), MapId, X, Y, Z, Orientation);
        SetState(EWowSessionState::WorldInGame);
    });

    SetState(EWowSessionState::WorldEnteringWorld);
    WorldSocket->SendPlayerLogin(G);
}

void UWowConnectionManager::CreateCharacter(const FString& Name, uint8 Race, uint8 Class, uint8 Gender,
    uint8 Skin, uint8 Face, uint8 HairStyle, uint8 HairColor, uint8 FacialHair)
{
    if (!WorldSocket.IsValid())
    {
        OnError.Broadcast(TEXT("Not connected to world server"));
        return;
    }

    WorldSocket->SendCharCreate(Name, Race, Class, Gender, Skin, Face, HairStyle, HairColor, FacialHair);
}

void UWowConnectionManager::DeleteCharacter(int64 Guid)
{
    if (!WorldSocket.IsValid())
    {
        OnError.Broadcast(TEXT("Not connected to world server"));
        return;
    }

    WorldSocket->SendCharDelete(static_cast<uint64>(Guid));
}

void UWowConnectionManager::SendMovement(int32 Opcode, const FVector& Position, float Orientation, int32 MoveFlags)
{
    if (!WorldSocket.IsValid() || State != EWowSessionState::WorldInGame) return;

    // Build movement info payload:
    // packedGuid + uint32 moveFlags + uint16 moveFlags2 + uint32 timestamp
    // + float x,y,z,orientation + uint32 fallTime
    uint64 Guid = PacketHandler.EntityManager.LocalPlayerGuid;

    // Pack GUID (variable-length)
    TArray<uint8> Data;
    Data.Reserve(64);

    // Packed GUID
    uint8 GuidMask = 0;
    uint8 GuidBytes[8];
    int32 GuidByteCount = 0;
    for (int32 i = 0; i < 8; ++i)
    {
        uint8 Byte = static_cast<uint8>((Guid >> (i * 8)) & 0xFF);
        if (Byte != 0)
        {
            GuidMask |= (1 << i);
            GuidBytes[GuidByteCount++] = Byte;
        }
    }
    Data.Add(GuidMask);
    Data.Append(GuidBytes, GuidByteCount);

    // Movement flags
    uint32 MF = static_cast<uint32>(MoveFlags);
    Data.Append(reinterpret_cast<const uint8*>(&MF), 4);

    // MoveFlags2 = 0
    uint16 MF2 = 0;
    Data.Append(reinterpret_cast<const uint8*>(&MF2), 2);

    // Timestamp
    uint32 Time = FPlatformTime::Cycles();
    Data.Append(reinterpret_cast<const uint8*>(&Time), 4);

    // Position (x, y, z, orientation) — WoW coordinates, not UE
    float X = static_cast<float>(Position.X);
    float Y = static_cast<float>(Position.Y);
    float Z = static_cast<float>(Position.Z);
    Data.Append(reinterpret_cast<const uint8*>(&X), 4);
    Data.Append(reinterpret_cast<const uint8*>(&Y), 4);
    Data.Append(reinterpret_cast<const uint8*>(&Z), 4);
    Data.Append(reinterpret_cast<const uint8*>(&Orientation), 4);

    // Fall time = 0
    uint32 FallTime = 0;
    Data.Append(reinterpret_cast<const uint8*>(&FallTime), 4);

    WorldSocket->SendPacket(static_cast<uint32>(Opcode), Data);
}

void UWowConnectionManager::SendChatMessage(const FString& Message, int32 Type, const FString& Language)
{
    if (!WorldSocket.IsValid() || State != EWowSessionState::WorldInGame) return;

    // WoW 3.3.5a chat messages are limited to 255 bytes (UTF-8)
    FString TruncatedMessage = Message.Left(255);

    TArray<uint8> Data;
    Data.Reserve(TruncatedMessage.Len() + 16);

    // uint32 type (SAY=1, YELL=6, WHISPER=7, CHANNEL=17)
    uint32 ChatType = static_cast<uint32>(Type);
    Data.Append(reinterpret_cast<const uint8*>(&ChatType), 4);

    // uint32 language (0 = universal/common)
    uint32 Lang = 0;
    Data.Append(reinterpret_cast<const uint8*>(&Lang), 4);

    // Null-terminated message string (UTF-8)
    FTCHARToUTF8 MsgUtf8(*TruncatedMessage);
    int32 MsgLen = FMath::Min(MsgUtf8.Length(), 255);
    Data.Append(reinterpret_cast<const uint8*>(MsgUtf8.Get()), MsgLen);
    Data.Add(0);

    WorldSocket->SendPacket(WowOpcode::CMSG_MESSAGECHAT, Data);
    UE_LOG(LogWowNet, Log, TEXT("Sent chat (type=%d): %s"), Type, *Message);
}

void UWowConnectionManager::SendSetSelection(int64 InTargetGuid)
{
    if (!WorldSocket.IsValid()) return;

    TargetGuid = InTargetGuid;

    TArray<uint8> Data;
    Data.SetNumUninitialized(8);
    uint64 G = static_cast<uint64>(InTargetGuid);
    FMemory::Memcpy(Data.GetData(), &G, 8);

    WorldSocket->SendPacket(WowOpcode::CMSG_SET_SELECTION, Data);
    UE_LOG(LogWowNet, Log, TEXT("Set target: GUID %lld"), InTargetGuid);
}

void UWowConnectionManager::SendCastSpell(int32 SpellId, int64 InTargetGuid)
{
    if (!WorldSocket.IsValid() || State != EWowSessionState::WorldInGame) return;

    TArray<uint8> Data;
    Data.Reserve(32);

    // castCount (uint8) — 0 for first cast
    uint8 CastCount = 0;
    Data.Add(CastCount);

    // spellId (uint32)
    uint32 Spell = static_cast<uint32>(SpellId);
    Data.Append(reinterpret_cast<const uint8*>(&Spell), 4);

    // castFlags (uint8) — 0 for normal cast
    uint8 CastFlags = 0;
    Data.Add(CastFlags);

    // targetMask (uint32) — TARGET_FLAG_UNIT (0x02) if we have a target, else TARGET_FLAG_NONE (0x00)
    uint64 TGuid = static_cast<uint64>(InTargetGuid);
    uint32 TargetMask = (TGuid != 0) ? 0x0002 : 0x0000;
    Data.Append(reinterpret_cast<const uint8*>(&TargetMask), 4);

    // If targeting a unit, write packed GUID
    if (TGuid != 0)
    {
        uint8 GuidMask = 0;
        uint8 GuidBytes[8];
        int32 GuidByteCount = 0;
        for (int32 i = 0; i < 8; ++i)
        {
            uint8 Byte = static_cast<uint8>((TGuid >> (i * 8)) & 0xFF);
            if (Byte != 0)
            {
                GuidMask |= (1 << i);
                GuidBytes[GuidByteCount++] = Byte;
            }
        }
        Data.Add(GuidMask);
        Data.Append(GuidBytes, GuidByteCount);
    }

    WorldSocket->SendPacket(WowOpcode::CMSG_CAST_SPELL, Data);
    UE_LOG(LogWowNet, Log, TEXT("Cast spell %d on target %lld"), SpellId, InTargetGuid);
}

void UWowConnectionManager::SendAttackSwing(int64 InTargetGuid)
{
    if (!WorldSocket.IsValid() || State != EWowSessionState::WorldInGame) return;

    TArray<uint8> Data;
    Data.SetNumUninitialized(8);
    uint64 G = static_cast<uint64>(InTargetGuid);
    FMemory::Memcpy(Data.GetData(), &G, 8);

    WorldSocket->SendPacket(WowOpcode::CMSG_ATTACKSWING, Data);
    UE_LOG(LogWowNet, Log, TEXT("Attack swing on target %lld"), InTargetGuid);
}

void UWowConnectionManager::SendAttackStop()
{
    if (!WorldSocket.IsValid() || State != EWowSessionState::WorldInGame) return;

    WorldSocket->SendPacket(WowOpcode::CMSG_ATTACKSTOP);
    UE_LOG(LogWowNet, Log, TEXT("Attack stop"));
}

void UWowConnectionManager::SendKeepAlive()
{
    if (!WorldSocket.IsValid()) return;
    WorldSocket->SendPacket(WowOpcode::CMSG_KEEP_ALIVE);
}

void UWowConnectionManager::SendRawPacket(uint32 Opcode, const TArray<uint8>& Data)
{
    if (!WorldSocket.IsValid()) return;
    WorldSocket->SendPacket(Opcode, Data);
}

void UWowConnectionManager::Disconnect()
{
    if (WorldSocket.IsValid())
    {
        WorldSocket->Disconnect();
        WorldSocket.Reset();
    }

    if (AuthSocket.IsValid())
    {
        AuthSocket->Disconnect();
        AuthSocket.Reset();
    }

    PacketHandler.EntityManager.Clear();
    PacketHandler.OnLoginVerifyWorld.Clear();
    PacketHandler.OnSendPacket.Unbind();

    SessionKey.Empty();
    CachedRealms.Empty();
    CachedCharacters.Empty();
    CachedAccountName.Empty();

    SetState(EWowSessionState::Disconnected);
}
