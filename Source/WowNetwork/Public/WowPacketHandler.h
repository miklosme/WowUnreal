#pragma once
#include "CoreMinimal.h"
#include "WowOpcodes.h"
#include "WowEntityManager.h"
#include "WowWardenHandler.h"

DECLARE_MULTICAST_DELEGATE_FiveParams(FOnLoginVerifyWorld, uint32 /*MapId*/, float /*X*/, float /*Y*/, float /*Z*/, float /*Orientation*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatMessage, const FString& /*Message*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnSpellStart, uint64 /*CasterGuid*/, uint32 /*SpellId*/, uint32 /*CastFlags*/, int32 /*CastTime*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLootOpened, uint64 /*LootGuid*/, const TArray<FWowLootItem>& /*Items*/);
DECLARE_DELEGATE_TwoParams(FOnSendPacket, uint32 /*Opcode*/, const TArray<uint8>& /*Data*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOpcodeReceived, uint16 /*Opcode*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnQuestAccepted, uint32 /*QuestId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnQuestComplete, uint32 /*QuestId*/);
DECLARE_MULTICAST_DELEGATE(FOnTalentsUpdated);
DECLARE_MULTICAST_DELEGATE(FOnFriendListUpdated);
DECLARE_MULTICAST_DELEGATE(FOnGuildRosterUpdated);
DECLARE_MULTICAST_DELEGATE(FOnGroupUpdated);
DECLARE_MULTICAST_DELEGATE_FiveParams(FOnTeleportRequest, uint64 /*Guid*/, uint32 /*Flags*/, uint32 /*Time*/, FVector /*Position*/, float /*Orientation*/);
DECLARE_MULTICAST_DELEGATE_FiveParams(FOnMapTransfer, uint32 /*MapId*/, float /*X*/, float /*Y*/, float /*Z*/, float /*Orientation*/);

// Simple byte-stream reader for packet payloads
struct FPacketReader
{
    const uint8* Data;
    int32 Size;
    int32 Pos = 0;

    FPacketReader(const TArray<uint8>& InData) : Data(InData.GetData()), Size(InData.Num()) {}
    FPacketReader(const uint8* InData, int32 InSize) : Data(InData), Size(InSize) {}

    bool CanRead(int32 Bytes) const { return Pos + Bytes <= Size; }
    int32 Remaining() const { return Size - Pos; }

    uint8 ReadU8() { return CanRead(1) ? Data[Pos++] : 0; }
    uint16 ReadU16()
    {
        if (!CanRead(2)) return 0;
        uint16 V = Data[Pos] | (uint16(Data[Pos + 1]) << 8);
        Pos += 2;
        return V;
    }
    uint32 ReadU32()
    {
        if (!CanRead(4)) return 0;
        uint32 V;
        FMemory::Memcpy(&V, Data + Pos, 4);
        Pos += 4;
        return V;
    }
    uint64 ReadU64()
    {
        if (!CanRead(8)) return 0;
        uint64 V;
        FMemory::Memcpy(&V, Data + Pos, 8);
        Pos += 8;
        return V;
    }
    float ReadFloat()
    {
        if (!CanRead(4)) return 0.0f;
        float V;
        FMemory::Memcpy(&V, Data + Pos, 4);
        Pos += 4;
        return V;
    }
    FString ReadCString()
    {
        FString Result;
        while (Pos < Size && Data[Pos] != 0)
        {
            Result += static_cast<TCHAR>(Data[Pos++]);
        }
        if (Pos < Size) Pos++; // skip null
        return Result;
    }
    void Skip(int32 Bytes) { Pos = FMath::Min(Pos + Bytes, Size); }

    // Read packed GUID (variable-length encoding)
    uint64 ReadPackedGuid()
    {
        uint8 Mask = ReadU8();
        uint64 Guid = 0;
        for (int32 i = 0; i < 8; ++i)
        {
            if (Mask & (1 << i))
            {
                Guid |= static_cast<uint64>(ReadU8()) << (i * 8);
            }
        }
        return Guid;
    }
};

class WOWNETWORK_API FWowPacketHandler
{
public:
    FWowPacketHandler();

    /** Handle an incoming server packet (called from game thread) */
    void HandlePacket(uint16 Opcode, const TArray<uint8>& Data);

    /** Entity manager — tracks all objects in the world */
    FWowEntityManager EntityManager;

    /** Known spell IDs (populated from SMSG_INITIAL_SPELLS) */
    TSet<uint32> KnownSpells;

    /** Quest log entries */
    TArray<FWowQuestLogEntry> QuestLog;

    /** Known talents */
    TArray<FWowTalentInfo> Talents;

    /** Warden anti-cheat handler */
    FWowWardenHandler WardenHandler;

    /** ── Social System ──────────────────────────────────────────────────── */
    /** Friends list */
    TArray<FWowFriendInfo> FriendsList;

    /** Guild roster */
    TArray<FWowGuildMember> GuildRoster;

    /** Guild information */
    FString GuildName;
    FString GuildMotd;

    // Events
    FOnLoginVerifyWorld OnLoginVerifyWorld;
    FOnChatMessage OnChatMessage;
    FOnSpellStart OnSpellStart;
    FOnLootOpened OnLootOpened;
    FOnQuestAccepted OnQuestAccepted;
    FOnQuestComplete OnQuestComplete;
    FOnTalentsUpdated OnTalentsUpdated;
    FOnFriendListUpdated OnFriendListUpdated;
    FOnGuildRosterUpdated OnGuildRosterUpdated;
    FOnGroupUpdated OnGroupUpdated;
    FOnTeleportRequest OnTeleportRequest;
    FOnMapTransfer OnMapTransfer;

    /** Bind this to send packets back to the server (e.g. TIME_SYNC_RESP) */
    FOnSendPacket OnSendPacket;

    /** Initialize Warden encryption with session key */
    void InitializeWarden(const TArray<uint8>& SessionKey);

    /** Fired for every processed SMSG opcode — wire to EventSystem for UI events */
    FOnOpcodeReceived OnOpcodeReceived;

private:
    // Handler function pointer type
    using HandlerFunc = void (FWowPacketHandler::*)(FPacketReader&);

    // Dispatch table
    TMap<uint16, HandlerFunc> Handlers;

    // Individual handlers
    void HandleLoginVerifyWorld(FPacketReader& R);
    void HandleUpdateObject(FPacketReader& R);
    void HandleCompressedUpdateObject(FPacketReader& R);
    void HandleDestroyObject(FPacketReader& R);
    void HandleMovement(FPacketReader& R);
    void HandleMessageChat(FPacketReader& R);
    void HandleInitialSpells(FPacketReader& R);
    void HandleActionButtons(FPacketReader& R);
    void HandleTimeSyncReq(FPacketReader& R);
    void HandleSpellStart(FPacketReader& R);
    void HandleSpellGo(FPacketReader& R);
    void HandleAuraUpdate(FPacketReader& R);
    void HandlePowerUpdate(FPacketReader& R);
    void HandleMonsterMove(FPacketReader& R);
    void HandleInventoryChangeFailure(FPacketReader& R);
    void HandleLootResponse(FPacketReader& R);
    void HandleLootReleaseResponse(FPacketReader& R);
    void HandleItemPushResult(FPacketReader& R);

    // Quest handlers
    void HandleQuestgiverStatus(FPacketReader& R);
    void HandleQuestgiverQuestDetails(FPacketReader& R);
    void HandleQuestgiverOfferReward(FPacketReader& R);
    void HandleQuestUpdateAddKill(FPacketReader& R);
    void HandleQuestUpdateComplete(FPacketReader& R);

    // Talent handlers
    void HandleTalentsInfo(FPacketReader& R);
    void HandleLearnedSpell(FPacketReader& R);
    void HandleRemovedSpell(FPacketReader& R);

    // ── Social / Guild / Friends handlers ──────────────────────────────────
    void HandleFriendList(FPacketReader& R);
    void HandleFriendStatus(FPacketReader& R);
    void HandleGuildRoster(FPacketReader& R);
    void HandleGuildEvent(FPacketReader& R);
    void HandleChannelNotify(FPacketReader& R);
    void HandleGroupList(FPacketReader& R);
    void HandlePartyCommandResult(FPacketReader& R);
    void HandleWho(FPacketReader& R);

    // ── Warden / Teleport handlers ──────────────────────────────────────────
    void HandleWardenData(FPacketReader& R);
    void HandleMoveTeleport(FPacketReader& R);
    void HandleTransferPending(FPacketReader& R);
    void HandleNewWorld(FPacketReader& R);

    // Internal parsing
    void ParseUpdateBlock(FPacketReader& R);
    void ParseMovementInfo(FPacketReader& R, FWowMovementInfo& Out);
    void ParseUpdateFields(FPacketReader& R, FWowEntity& Entity);

    // Internal Warden response sender (bound to WardenHandler)
    void SendWardenResponse(uint32 Opcode, const TArray<uint8>& Data);

    // Stats
    int32 EntitiesCreated = 0;
    int32 EntitiesUpdated = 0;
    int32 EntitiesDestroyed = 0;
};
