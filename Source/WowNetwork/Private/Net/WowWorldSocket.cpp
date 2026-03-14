#include "Net/WowWorldSocket.h"
#include "WowSessionState.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "HAL/RunnableThread.h"

#include "Crypto/OpenSSLIncludes.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowWorld, Log, All);

// ── Opcodes ────────────────────────────────────────────────────────────────────
// Client -> Server
static constexpr uint16 CMSG_AUTH_SESSION   = 0x01ED;
static constexpr uint16 CMSG_CHAR_ENUM     = 0x0037;
static constexpr uint16 CMSG_PLAYER_LOGIN  = 0x003D;
static constexpr uint16 CMSG_KEEP_ALIVE    = 0x0406;

// Server -> Client
static constexpr uint16 SMSG_AUTH_CHALLENGE      = 0x01EC;
static constexpr uint16 SMSG_AUTH_RESPONSE       = 0x01EE;
static constexpr uint16 SMSG_CHAR_ENUM           = 0x003B;
static constexpr uint16 SMSG_ADDON_INFO          = 0x02EF;
static constexpr uint16 SMSG_WARDEN_DATA         = 0x02E6;

// Auth result codes
static constexpr uint8 AUTH_OK = 0x0C;

// ── Construction / Destruction ─────────────────────────────────────────────────

FWowWorldSocket::FWowWorldSocket()
{
}

FWowWorldSocket::~FWowWorldSocket()
{
    Disconnect();
}

// ── Connect / Disconnect ───────────────────────────────────────────────────────

bool FWowWorldSocket::Connect(const FString& Host, int32 Port, const FString& InAccountName, const TArray<uint8>& InSessionKey, uint32 InRealmId)
{
    AccountName = InAccountName.ToUpper();
    SessionKey  = InSessionKey;
    RealmId = InRealmId;

    ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSub)
    {
        UE_LOG(LogWowWorld, Error, TEXT("Failed to get socket subsystem"));
        return false;
    }

    Socket = SocketSub->CreateSocket(NAME_Stream, TEXT("WowWorld"), false);
    if (!Socket)
    {
        UE_LOG(LogWowWorld, Error, TEXT("Failed to create socket"));
        return false;
    }

    TSharedRef<FInternetAddr> Addr = SocketSub->CreateInternetAddr();
    ESocketErrors ResolveError = SocketSub->GetHostByName(TCHAR_TO_ANSI(*Host), *Addr);
    if (ResolveError != SE_NO_ERROR)
    {
        UE_LOG(LogWowWorld, Error, TEXT("Failed to resolve host: %s"), *Host);
        SocketSub->DestroySocket(Socket);
        Socket = nullptr;
        return false;
    }

    Addr->SetPort(Port);

    if (!Socket->Connect(*Addr))
    {
        UE_LOG(LogWowWorld, Error, TEXT("Failed to connect to %s:%d"), *Host, Port);
        SocketSub->DestroySocket(Socket);
        Socket = nullptr;
        return false;
    }

    UE_LOG(LogWowWorld, Log, TEXT("Connected to world server %s:%d"), *Host, Port);

    bRunning   = true;
    bEncrypted = false;
    Thread = FRunnableThread::Create(this, TEXT("WowWorldSocket"), 0, TPri_Normal);

    return true;
}

void FWowWorldSocket::Disconnect()
{
    bRunning = false;

    {
        FScopeLock Lock(&SocketLock);
        if (Socket)
        {
            Socket->Close();
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
            Socket = nullptr;
        }
    }

    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }
}

// ── FRunnable ──────────────────────────────────────────────────────────────────

bool FWowWorldSocket::Init()
{
    return true;
}

uint32 FWowWorldSocket::Run()
{
    while (bRunning)
    {
        // ── Flush outgoing send queue ──────────────────────────────────────
        {
            TArray<uint8> Queued;
            while (SendQueue.Dequeue(Queued))
            {
                FScopeLock Lock(&SocketLock);
                if (!SendRawBytes(Queued.GetData(), Queued.Num()))
                {
                    UE_LOG(LogWowWorld, Error, TEXT("Send failed, disconnecting"));
                    bRunning = false;
                    return 1;
                }
            }
        }

        // ── Check if data available ────────────────────────────────────────
        {
            FScopeLock Lock(&SocketLock);
            if (!Socket)
            {
                bRunning = false;
                return 1;
            }

            uint32 PendingData = 0;
            if (!Socket->HasPendingData(PendingData) || PendingData == 0)
            {
                FPlatformProcess::Sleep(0.01f);
                continue;
            }
        }

        // ── Read packet header ─────────────────────────────────────────────
        if (!bEncrypted)
        {
            // Before encryption is initialised the ONLY packet we expect is
            // SMSG_AUTH_CHALLENGE which has an *unencrypted* server header:
            //   2 bytes size (big-endian) + 2 bytes opcode (little-endian)
            uint8 Header[4];
            if (!RecvExact(Header, 4))
            {
                UE_LOG(LogWowWorld, Error, TEXT("Failed to read unencrypted header"));
                bRunning = false;
                return 1;
            }

            uint16 Size   = (uint16(Header[0]) << 8) | Header[1];
            uint16 Opcode = Header[2] | (uint16(Header[3]) << 8);

            // Size includes the 2-byte opcode but not the size field itself
            int32 PayloadLen = Size - 2;
            TArray<uint8> Payload;
            if (PayloadLen > 0)
            {
                Payload.SetNumUninitialized(PayloadLen);
                if (!RecvExact(Payload.GetData(), PayloadLen))
                {
                    UE_LOG(LogWowWorld, Error, TEXT("Failed to read unencrypted payload"));
                    bRunning = false;
                    return 1;
                }
            }

            UE_LOG(LogWowWorld, Log, TEXT("Recv unencrypted opcode=0x%04X size=%d"), Opcode, Size);

            if (Opcode == SMSG_AUTH_CHALLENGE)
            {
                HandleAuthChallenge(Payload);
            }
            else
            {
                UE_LOG(LogWowWorld, Warning, TEXT("Unexpected unencrypted opcode: 0x%04X"), Opcode);
            }
        }
        else
        {
            // ── Encrypted server header ────────────────────────────────────
            // Normal: 4 bytes (2 size + 2 opcode)
            // Large:  5 bytes (3 size + 2 opcode) if high bit of first byte set
            uint8 Header[5];
            if (!RecvExact(Header, 4))
            {
                UE_LOG(LogWowWorld, Error, TEXT("Failed to read encrypted header"));
                bRunning = false;
                return 1;
            }

            Crypt.DecryptRecv(Header, 4);

            uint32 Size;
            uint16 Opcode;

            if (Header[0] & 0x80)
            {
                // Large packet -- need one more header byte
                if (!RecvExact(Header + 4, 1))
                {
                    UE_LOG(LogWowWorld, Error, TEXT("Failed to read large header byte"));
                    bRunning = false;
                    return 1;
                }
                Crypt.DecryptRecv(Header + 4, 1);

                // 3-byte big-endian size (mask out high bit)
                Size = (uint32(Header[0] & 0x7F) << 16) | (uint32(Header[1]) << 8) | uint32(Header[2]);
                Opcode = Header[3] | (uint16(Header[4]) << 8);
            }
            else
            {
                Size   = (uint16(Header[0]) << 8) | Header[1];
                Opcode = Header[2] | (uint16(Header[3]) << 8);
            }

            int32 PayloadLen = Size - 2; // size includes opcode (2 bytes) but not size field
            TArray<uint8> Payload;
            if (PayloadLen > 0)
            {
                Payload.SetNumUninitialized(PayloadLen);
                if (!RecvExact(Payload.GetData(), PayloadLen))
                {
                    UE_LOG(LogWowWorld, Error, TEXT("Failed to read encrypted payload (opcode=0x%04X, payloadLen=%d)"), Opcode, PayloadLen);
                    bRunning = false;
                    return 1;
                }
            }

            // Dispatch
            switch (Opcode)
            {
            case SMSG_AUTH_RESPONSE:
                HandleAuthResponse(Payload);
                break;
            case SMSG_CHAR_ENUM:
                HandleCharEnum(Payload);
                break;
            case SMSG_ADDON_INFO:
                UE_LOG(LogWowWorld, Log, TEXT("Received SMSG_ADDON_INFO (%d bytes) -- ignored"), Payload.Num());
                break;
            case SMSG_WARDEN_DATA:
                UE_LOG(LogWowWorld, Log, TEXT("Received SMSG_WARDEN_DATA (%d bytes) -- ignored"), Payload.Num());
                break;
            default:
                UE_LOG(LogWowWorld, Verbose, TEXT("Recv opcode=0x%04X size=%d"), Opcode, Size);
                AsyncTask(ENamedThreads::GameThread, [this, Opcode, Payload]()
                {
                    OnPacket.ExecuteIfBound(Opcode, Payload);
                });
                break;
            }
        }
    }

    return 0;
}

void FWowWorldSocket::Stop()
{
    bRunning = false;
}

// ── Auth Challenge Handler ─────────────────────────────────────────────────────
// Server sends: uint32(1) + uint8[4] serverSeed + uint8[16] seed1 + uint8[16] seed2

void FWowWorldSocket::HandleAuthChallenge(const TArray<uint8>& Data)
{
    if (Data.Num() < 4 + 4 + 16 + 16)
    {
        UE_LOG(LogWowWorld, Error, TEXT("AUTH_CHALLENGE too short: %d bytes"), Data.Num());
        AsyncTask(ENamedThreads::GameThread, [this]() { OnAuthResult.ExecuteIfBound(false); });
        bRunning = false;
        return;
    }

    // Skip uint32 "one" at offset 0
    // Server seed is 4 bytes at offset 4
    uint8 ServerSeed[4];
    FMemory::Memcpy(ServerSeed, Data.GetData() + 4, 4);

    // Generate client seed (random 4 bytes)
    uint8 ClientSeed[4];
    for (int32 i = 0; i < 4; ++i)
    {
        ClientSeed[i] = static_cast<uint8>(FMath::Rand() & 0xFF);
    }

    // ── Compute digest ─────────────────────────────────────────────────────
    // SHA1(accountName + {0,0,0,0} + clientSeed_LE + serverSeed_LE + sessionKey)
    FTCHARToUTF8 AccountUtf8(*AccountName);
    const uint8* AccountBytes = reinterpret_cast<const uint8*>(AccountUtf8.Get());
    int32 AccountLen = AccountUtf8.Length();

    uint8 ZeroBytes[4] = {0, 0, 0, 0};

    SHA_CTX Sha;
    SHA1_Init(&Sha);
    SHA1_Update(&Sha, AccountBytes, AccountLen);
    SHA1_Update(&Sha, ZeroBytes, 4);
    SHA1_Update(&Sha, ClientSeed, 4);
    SHA1_Update(&Sha, ServerSeed, 4);
    SHA1_Update(&Sha, SessionKey.GetData(), SessionKey.Num());

    uint8 Digest[20];
    SHA1_Final(Digest, &Sha);

    // ── Build CMSG_AUTH_SESSION payload ─────────────────────────────────────
    TArray<uint8> Payload;
    Payload.Reserve(256);

    // uint32 build = 12340
    uint32 Build = 12340;
    Payload.Append(reinterpret_cast<const uint8*>(&Build), 4);

    // uint32 loginServerId = 0
    uint32 LoginServerId = 0;
    Payload.Append(reinterpret_cast<const uint8*>(&LoginServerId), 4);

    // string accountName (null-terminated)
    Payload.Append(AccountBytes, AccountLen);
    Payload.Add(0);

    // uint32 loginServerType = 0
    uint32 LoginServerType = 0;
    Payload.Append(reinterpret_cast<const uint8*>(&LoginServerType), 4);

    // uint32 clientSeed
    Payload.Append(ClientSeed, 4);

    // uint32 regionId = 0
    uint32 RegionId = 0;
    Payload.Append(reinterpret_cast<const uint8*>(&RegionId), 4);

    // uint32 battleGroupId = 0
    uint32 BattleGroupId = 0;
    Payload.Append(reinterpret_cast<const uint8*>(&BattleGroupId), 4);

    // uint32 realmId
    Payload.Append(reinterpret_cast<const uint8*>(&RealmId), 4);

    // uint64 dosResponse = 0
    uint64 DosResponse = 0;
    Payload.Append(reinterpret_cast<const uint8*>(&DosResponse), 8);

    // uint8 digest[20]
    Payload.Append(Digest, 20);

    // Addon info: a minimal zlib-compressed addon block.
    // The server reads a uint32 decompressed size, then compressed data.
    // Simplest valid addon data: decompressed size = 4, data = uint32(0) meaning 0 addons.
    // Compress {0x00,0x00,0x00,0x00} with zlib:
    // We'll just send decompressed_size=0 to indicate no addon data.
    uint32 AddonDecompressedSize = 0;
    Payload.Append(reinterpret_cast<const uint8*>(&AddonDecompressedSize), 4);

    // ── Send as unencrypted client packet ──────────────────────────────────
    // Client packet header: 2 bytes size (big-endian) + 4 bytes opcode (little-endian)
    // Size = payload length + 4 (opcode size)
    uint16 PacketSize = static_cast<uint16>(Payload.Num() + 4);

    TArray<uint8> Packet;
    Packet.Reserve(2 + 4 + Payload.Num());

    // Size (big-endian)
    Packet.Add(static_cast<uint8>((PacketSize >> 8) & 0xFF));
    Packet.Add(static_cast<uint8>(PacketSize & 0xFF));

    // Opcode (little-endian, 4 bytes)
    uint32 OpcodeLE = CMSG_AUTH_SESSION;
    Packet.Append(reinterpret_cast<const uint8*>(&OpcodeLE), 4);

    // Payload
    Packet.Append(Payload);

    {
        FScopeLock Lock(&SocketLock);
        if (!SendRawBytes(Packet.GetData(), Packet.Num()))
        {
            UE_LOG(LogWowWorld, Error, TEXT("Failed to send CMSG_AUTH_SESSION"));
            AsyncTask(ENamedThreads::GameThread, [this]() { OnAuthResult.ExecuteIfBound(false); });
            bRunning = false;
            return;
        }
    }

    UE_LOG(LogWowWorld, Log, TEXT("Sent CMSG_AUTH_SESSION for account: %s realm=%u (payload %d bytes)"), *AccountName, RealmId, Payload.Num());

    // Initialise encryption AFTER sending AUTH_SESSION (the response will be encrypted)
    Crypt.Init(SessionKey);
    bEncrypted = true;

    UE_LOG(LogWowWorld, Log, TEXT("Encryption initialised"));
}

// ── Auth Response Handler ──────────────────────────────────────────────────────

void FWowWorldSocket::HandleAuthResponse(const TArray<uint8>& Data)
{
    if (Data.Num() < 1)
    {
        UE_LOG(LogWowWorld, Error, TEXT("AUTH_RESPONSE too short"));
        AsyncTask(ENamedThreads::GameThread, [this]() { OnAuthResult.ExecuteIfBound(false); });
        bRunning = false;
        return;
    }

    uint8 Result = Data[0];

    if (Result == AUTH_OK)
    {
        UE_LOG(LogWowWorld, Log, TEXT("World authentication successful (AUTH_OK)"));
        AsyncTask(ENamedThreads::GameThread, [this]() { OnAuthResult.ExecuteIfBound(true); });
    }
    else
    {
        UE_LOG(LogWowWorld, Error, TEXT("World authentication failed: result=0x%02X"), Result);
        AsyncTask(ENamedThreads::GameThread, [this]() { OnAuthResult.ExecuteIfBound(false); });
    }
}

// ── Character Enum Handler ─────────────────────────────────────────────────────

void FWowWorldSocket::HandleCharEnum(const TArray<uint8>& Data)
{
    if (Data.Num() < 1)
    {
        UE_LOG(LogWowWorld, Error, TEXT("CHAR_ENUM too short"));
        return;
    }

    uint8 NumChars = Data[0];
    int32 Offset = 1;

    TArray<FWowCharacterInfo> Characters;
    Characters.Reserve(NumChars);

    for (int32 i = 0; i < NumChars && Offset < Data.Num(); ++i)
    {
        FWowCharacterInfo Info;

        // uint64 guid
        if (Offset + 8 > Data.Num()) break;
        FMemory::Memcpy(&Info.Guid, Data.GetData() + Offset, 8);
        Offset += 8;

        // null-terminated name
        FString Name;
        while (Offset < Data.Num() && Data[Offset] != 0)
        {
            Name += static_cast<TCHAR>(Data[Offset++]);
        }
        Offset++; // skip null terminator
        Info.Name = Name;

        // uint8 race, class, gender
        if (Offset + 3 > Data.Num()) break;
        Info.Race   = Data[Offset++];
        Info.Class  = Data[Offset++];
        Info.Gender = Data[Offset++];

        // uint8 skin, face, hairStyle, hairColor, facialHair
        if (Offset + 5 > Data.Num()) break;
        Offset += 5; // skip appearance

        // uint8 level
        if (Offset + 1 > Data.Num()) break;
        Info.Level = Data[Offset++];

        // uint32 zoneId
        if (Offset + 4 > Data.Num()) break;
        FMemory::Memcpy(&Info.ZoneId, Data.GetData() + Offset, 4);
        Offset += 4;

        // uint32 mapId
        if (Offset + 4 > Data.Num()) break;
        FMemory::Memcpy(&Info.MapId, Data.GetData() + Offset, 4);
        Offset += 4;

        // float x, y, z
        if (Offset + 12 > Data.Num()) break;
        float X, Y, Z;
        FMemory::Memcpy(&X, Data.GetData() + Offset, 4); Offset += 4;
        FMemory::Memcpy(&Y, Data.GetData() + Offset, 4); Offset += 4;
        FMemory::Memcpy(&Z, Data.GetData() + Offset, 4); Offset += 4;
        Info.Position = FVector(X, Y, Z);

        // uint32 guildId
        if (Offset + 4 > Data.Num()) break;
        Offset += 4;

        // uint32 charFlags
        if (Offset + 4 > Data.Num()) break;
        Offset += 4;

        // uint32 firstLogin (atLoginFlags)
        if (Offset + 4 > Data.Num()) break;
        Offset += 4;

        // uint32 petDisplayId, petLevel, petFamily
        if (Offset + 12 > Data.Num()) break;
        Offset += 12;

        // Equipment: 23 slots × (uint32 displayId + uint8 invType + uint32 enchant) = 23 × 9 = 207 bytes
        // In 3.3.5 SMSG_CHAR_ENUM each slot is: displayInfoID(4) + inventoryType(1) + auraId(4) = 9 bytes × 23 slots
        int32 EquipmentBytes = 23 * 9;
        if (Offset + EquipmentBytes > Data.Num()) break;
        Offset += EquipmentBytes;

        Characters.Add(Info);
        UE_LOG(LogWowWorld, Log, TEXT("Character: %s (Level %d, Race %d, Class %d, GUID %llu)"),
               *Info.Name, Info.Level, Info.Race, Info.Class, Info.Guid);
    }

    UE_LOG(LogWowWorld, Log, TEXT("Received %d characters"), Characters.Num());

    AsyncTask(ENamedThreads::GameThread, [this, Characters]()
    {
        OnCharacterList.ExecuteIfBound(Characters);
    });
}

// ── Send Helpers ───────────────────────────────────────────────────────────────

void FWowWorldSocket::SendCharEnum()
{
    UE_LOG(LogWowWorld, Log, TEXT("Sending CMSG_CHAR_ENUM"));
    SendPacket(CMSG_CHAR_ENUM);
}

void FWowWorldSocket::SendPlayerLogin(uint64 Guid)
{
    TArray<uint8> Data;
    Data.SetNumUninitialized(8);
    FMemory::Memcpy(Data.GetData(), &Guid, 8);

    UE_LOG(LogWowWorld, Log, TEXT("Sending CMSG_PLAYER_LOGIN for GUID %llu"), Guid);
    SendPacket(CMSG_PLAYER_LOGIN, Data);
}

void FWowWorldSocket::SendPacket(uint32 Opcode, const TArray<uint8>& Data)
{
    // Client packet header: 2 bytes size (big-endian) + 4 bytes opcode (little-endian)
    // Size = payload + 4 (opcode)
    uint16 PacketSize = static_cast<uint16>(Data.Num() + 4);

    TArray<uint8> Packet;
    Packet.SetNumUninitialized(2 + 4 + Data.Num());

    // Size (big-endian)
    Packet[0] = static_cast<uint8>((PacketSize >> 8) & 0xFF);
    Packet[1] = static_cast<uint8>(PacketSize & 0xFF);

    // Opcode (little-endian, 4 bytes)
    FMemory::Memcpy(Packet.GetData() + 2, &Opcode, 4);

    // Payload
    if (Data.Num() > 0)
    {
        FMemory::Memcpy(Packet.GetData() + 6, Data.GetData(), Data.Num());
    }

    // Encrypt the header (first 6 bytes: 2 size + 4 opcode)
    if (bEncrypted)
    {
        Crypt.EncryptSend(Packet.GetData(), 6);
    }

    SendQueue.Enqueue(MoveTemp(Packet));
}

// ── Raw I/O ────────────────────────────────────────────────────────────────────

bool FWowWorldSocket::SendRawBytes(const uint8* Data, int32 Len)
{
    if (!Socket) return false;

    int32 TotalSent = 0;
    while (TotalSent < Len)
    {
        int32 Sent = 0;
        if (!Socket->Send(Data + TotalSent, Len - TotalSent, Sent))
        {
            return false;
        }
        if (Sent <= 0) return false;
        TotalSent += Sent;
    }
    return true;
}

bool FWowWorldSocket::RecvExact(uint8* Buffer, int32 Len)
{
    int32 TotalRead = 0;
    while (TotalRead < Len && bRunning)
    {
        int32 BytesRead = 0;
        FScopeLock Lock(&SocketLock);
        if (!Socket) return false;
        if (!Socket->Recv(Buffer + TotalRead, Len - TotalRead, BytesRead))
        {
            return false;
        }
        if (BytesRead == 0)
        {
            FPlatformProcess::Sleep(0.01f);
            continue;
        }
        TotalRead += BytesRead;
    }
    return TotalRead == Len;
}
