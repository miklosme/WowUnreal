#pragma once
#include "CoreMinimal.h"
#include "WowOpcodes.h"
#include "WowEntityManager.h"
#include "WowWardenHandler.h"

DECLARE_MULTICAST_DELEGATE_FiveParams(FOnLoginVerifyWorld, uint32 /*MapId*/, float /*X*/, float /*Y*/, float /*Z*/, float /*Orientation*/);
DECLARE_MULTICAST_DELEGATE_SixParams(FOnChatMessage, uint8 /*Type*/, uint32 /*Language*/, uint64 /*SenderGuid*/, const FString& /*SenderName*/, const FString& /*Message*/, const FString& /*Channel*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnSpellStart, uint64 /*CasterGuid*/, uint32 /*SpellId*/, uint32 /*CastFlags*/, int32 /*CastTime*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSpellGo, uint64 /*CasterGuid*/, uint32 /*SpellId*/, uint32 /*CastFlags*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnSpellFailure, uint64 /*CasterGuid*/, uint32 /*SpellId*/, uint8 /*FailureReason*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSpellCooldown, uint32 /*SpellId*/, float /*Duration*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnAttackerStateUpdate, uint64 /*AttackerGuid*/, uint64 /*TargetGuid*/, uint32 /*HitInfo*/, uint32 /*Damage*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnLootOpened, uint64 /*LootGuid*/, uint8 /*LootType*/, uint32 /*Gold*/, const TArray<FWowLootItem>& /*Items*/);
DECLARE_MULTICAST_DELEGATE(FOnLootClosed);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnVendorOpened, uint64 /*VendorGuid*/, const TArray<FWowVendorItem>& /*Items*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnQuestDialog, const FWowQuestDetails& /*QuestDetails*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnQuestRewardDialog, const FWowQuestDetails& /*QuestDetails*/);
DECLARE_DELEGATE_TwoParams(FOnSendPacket, uint32 /*Opcode*/, const TArray<uint8>& /*Data*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnOpcodeReceived, uint16 /*Opcode*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnQuestAccepted, uint32 /*QuestId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnQuestComplete, uint32 /*QuestId*/);
DECLARE_MULTICAST_DELEGATE(FOnTalentsUpdated);
DECLARE_MULTICAST_DELEGATE(FOnFriendListUpdated);
DECLARE_MULTICAST_DELEGATE(FOnGuildRosterUpdated);
DECLARE_MULTICAST_DELEGATE(FOnGroupUpdated);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnGroupInviteReceived, const FString& /*InviterName*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPartyCommandResult, uint8 /*Command*/, const FString& /*PlayerName*/, uint8 /*Result*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTaxiNodesShown, uint64 /*NpcGuid*/, uint32 /*CurrentNodeId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnTaxiActivateReply, uint8 /*Result*/);
DECLARE_MULTICAST_DELEGATE_FiveParams(FOnTeleportRequest, uint64 /*Guid*/, uint32 /*Flags*/, uint32 /*Time*/, FVector /*Position*/, float /*Orientation*/);
DECLARE_MULTICAST_DELEGATE_FiveParams(FOnMapTransfer, uint32 /*MapId*/, float /*X*/, float /*Y*/, float /*Z*/, float /*Orientation*/);
DECLARE_MULTICAST_DELEGATE(FOnPlayerDeath);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnEntityDeath, uint64 /*Guid*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCorpseReclaimDelay, float /*DelayInSeconds*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnResurrectRequest, const FString& /*RequesterName*/);
DECLARE_MULTICAST_DELEGATE_FourParams(FOnPlayerTeleport, uint32 /*MapId*/, float /*X*/, float /*Y*/, float /*Z*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPlayerNameReceived, uint64 /*Guid*/, const FString& /*Name*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCreatureNameReceived, uint32 /*Entry*/, const FString& /*Name*/, const FString& /*Title*/);
DECLARE_MULTICAST_DELEGATE(FOnPlayerInventoryUpdate);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEmote, uint64 /*EntityGuid*/, uint32 /*EmoteId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLevelUp, uint32 /*NewLevel*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnXPGain, uint32 /*Amount*/, uint8 /*Type*/);
DECLARE_MULTICAST_DELEGATE_FiveParams(FOnBindPointUpdate, float /*X*/, float /*Y*/, float /*Z*/, uint32 /*MapId*/, uint32 /*AreaId*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWeatherUpdate, uint32 /*WeatherType*/, float /*Grade*/, uint8 /*Sound*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWorldStatesInit, uint32 /*MapId*/, uint32 /*AreaId*/, uint32 /*SubAreaId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWorldStateUpdate, uint32 /*Field*/, uint32 /*Value*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProficiencySet, uint8 /*ItemClass*/, uint32 /*ItemSubclassMask*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnGossipMessage, uint64 /*NpcGuid*/, uint32 /*TextId*/, uint32 /*MenuId*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInitialSpells, const TArray<uint32>& /*SpellIds*/);
DECLARE_MULTICAST_DELEGATE(FOnActionButtonsUpdated);

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

    /** Action bar data (144 slots, first 12 are main action bar) */
    TArray<uint32> ActionButtons;

    /** Spell cooldowns (SpellId -> expiry time as FPlatformTime::Seconds()) */
    TMap<uint32, double> SpellCooldowns;

    /** Player name cache */
    TMap<uint64, FString> PlayerNameCache;

    /** Creature name cache */
    TMap<uint32, FString> CreatureNameCache;
    TMap<uint32, FString> CreatureTitleCache;

    /** World states (field -> value) */
    TMap<uint32, uint32> WorldStates;

    /** ── Social System ──────────────────────────────────────────────────── */
    /** Friends list */
    TArray<FWowFriendInfo> FriendsList;

    /** Guild roster */
    TArray<FWowGuildMember> GuildRoster;

    /** Guild information */
    FString GuildName;
    FString GuildMotd;

    /** Group/Party information */
    FWowGroupInfo GroupInfo;

    /** Taxi system data */
    FWowTaxiData TaxiData;

    // Events
    FOnLoginVerifyWorld OnLoginVerifyWorld;
    FOnChatMessage OnChatMessage;
    FOnSpellStart OnSpellStart;
    FOnSpellGo OnSpellGo;
    FOnSpellFailure OnSpellFailure;
    FOnSpellCooldown OnSpellCooldown;
    FOnAttackerStateUpdate OnAttackerStateUpdate;
    FOnLootOpened OnLootOpened;
    FOnLootClosed OnLootClosed;
    FOnVendorOpened OnVendorOpened;
    FOnQuestDialog OnQuestDialog;
    FOnQuestRewardDialog OnQuestRewardDialog;
    FOnQuestAccepted OnQuestAccepted;
    FOnQuestComplete OnQuestComplete;
    FOnTalentsUpdated OnTalentsUpdated;
    FOnFriendListUpdated OnFriendListUpdated;
    FOnGuildRosterUpdated OnGuildRosterUpdated;
    FOnGroupUpdated OnGroupUpdated;
    FOnGroupInviteReceived OnGroupInviteReceived;
    FOnPartyCommandResult OnPartyCommandResult;
    FOnTaxiNodesShown OnTaxiNodesShown;
    FOnTaxiActivateReply OnTaxiActivateReply;
    FOnTeleportRequest OnTeleportRequest;
    FOnMapTransfer OnMapTransfer;
    FOnPlayerDeath OnPlayerDeath;
    FOnEntityDeath OnEntityDeath;
    FOnCorpseReclaimDelay OnCorpseReclaimDelay;
    FOnResurrectRequest OnResurrectRequest;
    FOnPlayerTeleport OnPlayerTeleport;
    FOnPlayerNameReceived OnPlayerNameReceived;
    FOnCreatureNameReceived OnCreatureNameReceived;
    FOnPlayerInventoryUpdate OnPlayerInventoryUpdate;
    FOnEmote OnEmote;
    FOnLevelUp OnLevelUp;
    FOnXPGain OnXPGain;
    FOnBindPointUpdate OnBindPointUpdate;
    FOnWeatherUpdate OnWeatherUpdate;
    FOnWorldStatesInit OnWorldStatesInit;
    FOnWorldStateUpdate OnWorldStateUpdate;
    FOnProficiencySet OnProficiencySet;
    FOnGossipMessage OnGossipMessage;
    FOnInitialSpells OnInitialSpells;
    FOnActionButtonsUpdated OnActionButtonsUpdated;

    /** Bind this to send packets back to the server (e.g. TIME_SYNC_RESP) */
    FOnSendPacket OnSendPacket;

    /** Initialize Warden encryption with session key */
    void InitializeWarden(const TArray<uint8>& SessionKey);

    /** Check if spell is on cooldown */
    bool IsSpellOnCooldown(uint32 SpellId) const;

    /** Get remaining cooldown time in seconds */
    float GetSpellCooldownRemaining(uint32 SpellId) const;

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
    void HandleSpellFailure(FPacketReader& R);
    void HandleSpellCooldown(FPacketReader& R);
    void HandleAttackerStateUpdate(FPacketReader& R);
    void HandleAuraUpdate(FPacketReader& R);
    void HandlePowerUpdate(FPacketReader& R);
    void HandleMonsterMove(FPacketReader& R);
    void HandleInventoryChangeFailure(FPacketReader& R);
    void HandleLootResponse(FPacketReader& R);
    void HandleLootReleaseResponse(FPacketReader& R);
    void HandleItemPushResult(FPacketReader& R);
    void HandleListInventory(FPacketReader& R);

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
    void HandleGroupInvite(FPacketReader& R);
    void HandleWho(FPacketReader& R);

    // ── Taxi / Flight Path handlers ─────────────────────────────────────────
    void HandleShowTaxiNodes(FPacketReader& R);
    void HandleActivateTaxiReply(FPacketReader& R);
    void HandleNewTaxiPath(FPacketReader& R);
    void HandleNameQueryResponse(FPacketReader& R);
    void HandleCreatureQueryResponse(FPacketReader& R);

    // ── Emote handlers ──────────────────────────────────────────────────────
    void HandleEmote(FPacketReader& R);
    void HandleTextEmote(FPacketReader& R);

    // ── Warden / Teleport handlers ──────────────────────────────────────────
    void HandleWardenData(FPacketReader& R);
    void HandleMoveTeleport(FPacketReader& R);
    void HandleTransferPending(FPacketReader& R);
    void HandleNewWorld(FPacketReader& R);

    // ── Death / Corpse / Resurrection handlers ──────────────────────────────
    void HandleCorpseReclaimDelay(FPacketReader& R);
    void HandleResurrectRequest(FPacketReader& R);

    // ── Player progression handlers ─────────────────────────────────────────
    void HandleLevelUpInfo(FPacketReader& R);
    void HandleLogXPGain(FPacketReader& R);
    void HandleExplorationExperience(FPacketReader& R);
    void HandleEnvironmentalDamageLog(FPacketReader& R);
    void HandleBindPointUpdate(FPacketReader& R);
    void HandlePlayedTime(FPacketReader& R);

    // ── World state handlers ────────────────────────────────────────────────
    void HandleWeather(FPacketReader& R);
    void HandleInitWorldStates(FPacketReader& R);
    void HandleUpdateWorldState(FPacketReader& R);

    // ── Proficiency handlers ────────────────────────────────────────────────
    void HandleSetProficiency(FPacketReader& R);
    void HandleAccountDataTimes(FPacketReader& R);
    void HandlePartyMemberStats(FPacketReader& R);

    // ── NPC interaction handlers ────────────────────────────────────────────
    void HandleGossipMessage(FPacketReader& R);

    // ── Mail system handlers ────────────────────────────────────────────────
    void HandleMailListResult(FPacketReader& R);

    // ── Bank system handlers ────────────────────────────────────────────────
    void HandleShowBank(FPacketReader& R);

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
