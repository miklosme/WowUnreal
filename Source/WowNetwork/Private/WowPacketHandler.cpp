#include "WowPacketHandler.h"
#include "WowOpcodes.h"
#include "WowUpdateFields.h"
#include "WowWardenHandler.h"
#include "Compression.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowPacket, Log, All);

FWowPacketHandler::FWowPacketHandler()
{
    // Register handlers
    Handlers.Add(WowOpcode::SMSG_LOGIN_VERIFY_WORLD,      &FWowPacketHandler::HandleLoginVerifyWorld);
    Handlers.Add(WowOpcode::SMSG_UPDATE_OBJECT,            &FWowPacketHandler::HandleUpdateObject);
    Handlers.Add(WowOpcode::SMSG_COMPRESSED_UPDATE_OBJECT, &FWowPacketHandler::HandleCompressedUpdateObject);
    Handlers.Add(WowOpcode::SMSG_DESTROY_OBJECT,           &FWowPacketHandler::HandleDestroyObject);
    Handlers.Add(WowOpcode::SMSG_MESSAGECHAT,              &FWowPacketHandler::HandleMessageChat);
    Handlers.Add(WowOpcode::SMSG_INITIAL_SPELLS,           &FWowPacketHandler::HandleInitialSpells);
    Handlers.Add(WowOpcode::SMSG_ACTION_BUTTONS,           &FWowPacketHandler::HandleActionButtons);
    Handlers.Add(WowOpcode::SMSG_TIME_SYNC_REQ,            &FWowPacketHandler::HandleTimeSyncReq);
    Handlers.Add(WowOpcode::SMSG_SPELL_START,              &FWowPacketHandler::HandleSpellStart);
    Handlers.Add(WowOpcode::SMSG_SPELL_GO,                 &FWowPacketHandler::HandleSpellGo);
    Handlers.Add(WowOpcode::SMSG_SPELL_FAILURE,            &FWowPacketHandler::HandleSpellFailure);
    Handlers.Add(WowOpcode::SMSG_ATTACKERSTATEUPDATE,      &FWowPacketHandler::HandleAttackerStateUpdate);
    Handlers.Add(WowOpcode::SMSG_AURA_UPDATE,              &FWowPacketHandler::HandleAuraUpdate);
    Handlers.Add(WowOpcode::SMSG_POWER_UPDATE,             &FWowPacketHandler::HandlePowerUpdate);
    Handlers.Add(WowOpcode::SMSG_MONSTER_MOVE,             &FWowPacketHandler::HandleMonsterMove);

    // Inventory system handlers
    Handlers.Add(WowOpcode::SMSG_INVENTORY_CHANGE_FAILURE, &FWowPacketHandler::HandleInventoryChangeFailure);
    Handlers.Add(WowOpcode::SMSG_LOOT_RESPONSE,            &FWowPacketHandler::HandleLootResponse);
    Handlers.Add(WowOpcode::SMSG_LOOT_RELEASE_RESPONSE,    &FWowPacketHandler::HandleLootReleaseResponse);
    Handlers.Add(WowOpcode::SMSG_ITEM_PUSH_RESULT,         &FWowPacketHandler::HandleItemPushResult);

    // Vendor system handlers
    Handlers.Add(WowOpcode::SMSG_LIST_INVENTORY,           &FWowPacketHandler::HandleListInventory);

    // Quest system handlers
    Handlers.Add(WowOpcode::SMSG_QUESTGIVER_STATUS,        &FWowPacketHandler::HandleQuestgiverStatus);
    Handlers.Add(WowOpcode::SMSG_QUESTGIVER_QUEST_DETAILS, &FWowPacketHandler::HandleQuestgiverQuestDetails);
    Handlers.Add(WowOpcode::SMSG_QUESTGIVER_OFFER_REWARD,  &FWowPacketHandler::HandleQuestgiverOfferReward);
    Handlers.Add(WowOpcode::SMSG_QUEST_UPDATE_ADD_KILL,    &FWowPacketHandler::HandleQuestUpdateAddKill);
    Handlers.Add(WowOpcode::SMSG_QUEST_UPDATE_COMPLETE,    &FWowPacketHandler::HandleQuestUpdateComplete);

    // Talent system handlers
    Handlers.Add(WowOpcode::SMSG_TALENTS_INFO,             &FWowPacketHandler::HandleTalentsInfo);
    Handlers.Add(WowOpcode::SMSG_LEARNED_SPELL,            &FWowPacketHandler::HandleLearnedSpell);
    Handlers.Add(WowOpcode::SMSG_REMOVED_SPELL,            &FWowPacketHandler::HandleRemovedSpell);

    // ── Social / Guild / Friends handlers ──────────────────────────────────
    Handlers.Add(WowOpcode::SMSG_FRIEND_LIST,              &FWowPacketHandler::HandleFriendList);
    Handlers.Add(WowOpcode::SMSG_FRIEND_STATUS,            &FWowPacketHandler::HandleFriendStatus);
    Handlers.Add(WowOpcode::SMSG_GUILD_ROSTER,             &FWowPacketHandler::HandleGuildRoster);
    Handlers.Add(WowOpcode::SMSG_GUILD_EVENT,              &FWowPacketHandler::HandleGuildEvent);
    Handlers.Add(WowOpcode::SMSG_CHANNEL_NOTIFY,           &FWowPacketHandler::HandleChannelNotify);
    Handlers.Add(WowOpcode::SMSG_GROUP_LIST,               &FWowPacketHandler::HandleGroupList);
    Handlers.Add(WowOpcode::SMSG_PARTY_COMMAND_RESULT,     &FWowPacketHandler::HandlePartyCommandResult);
    Handlers.Add(WowOpcode::SMSG_GROUP_INVITE,             &FWowPacketHandler::HandleGroupInvite);
    Handlers.Add(WowOpcode::SMSG_WHO,                      &FWowPacketHandler::HandleWho);

    // ── Taxi / Flight Path handlers ─────────────────────────────────────────
    Handlers.Add(WowOpcode::SMSG_SHOWTAXINODES,            &FWowPacketHandler::HandleShowTaxiNodes);
    Handlers.Add(WowOpcode::SMSG_ACTIVATETAXIREPLY,        &FWowPacketHandler::HandleActivateTaxiReply);
    Handlers.Add(WowOpcode::SMSG_NEW_TAXI_PATH,            &FWowPacketHandler::HandleNewTaxiPath);

    // ── Death / Corpse / Resurrection handlers ──────────────────────────────
    Handlers.Add(WowOpcode::SMSG_CORPSE_RECLAIM_DELAY,     &FWowPacketHandler::HandleCorpseReclaimDelay);
    Handlers.Add(WowOpcode::SMSG_RESURRECT_REQUEST,        &FWowPacketHandler::HandleResurrectRequest);
    Handlers.Add(WowOpcode::SMSG_MOVE_TELEPORT,            &FWowPacketHandler::HandleMoveTeleport);

    // ── Name query handlers ─────────────────────────────────────────────────
    Handlers.Add(WowOpcode::SMSG_NAME_QUERY_RESPONSE,      &FWowPacketHandler::HandleNameQueryResponse);
    Handlers.Add(WowOpcode::SMSG_CREATURE_QUERY_RESPONSE,  &FWowPacketHandler::HandleCreatureQueryResponse);

    // Movement handlers — all use the same parser
    for (uint16 Op = WowOpcode::MSG_MOVE_START_FORWARD; Op <= WowOpcode::MSG_MOVE_SET_PITCH; ++Op)
    {
        Handlers.Add(Op, &FWowPacketHandler::HandleMovement);
    }
    Handlers.Add(WowOpcode::MSG_MOVE_ROOT, &FWowPacketHandler::HandleMovement);
    Handlers.Add(WowOpcode::MSG_MOVE_UNROOT, &FWowPacketHandler::HandleMovement);
    Handlers.Add(WowOpcode::MSG_MOVE_HEARTBEAT, &FWowPacketHandler::HandleMovement);

    // Warden and teleport handlers
    Handlers.Add(WowOpcode::SMSG_WARDEN_DATA,       &FWowPacketHandler::HandleWardenData);
    Handlers.Add(WowOpcode::MSG_MOVE_TELEPORT,      &FWowPacketHandler::HandleMoveTeleport);
    Handlers.Add(WowOpcode::SMSG_TRANSFER_PENDING,  &FWowPacketHandler::HandleTransferPending);
    Handlers.Add(WowOpcode::SMSG_NEW_WORLD,         &FWowPacketHandler::HandleNewWorld);

    // Bind Warden response delegate to send packets
    WardenHandler.OnSendResponse.BindLambda([this](uint32 Opcode, const TArray<uint8>& Data)
    {
        SendWardenResponse(Opcode, Data);
    });
}

void FWowPacketHandler::HandlePacket(uint16 Opcode, const TArray<uint8>& Data)
{
    // Validate opcode is in valid WoW 3.3.5a range (max opcode ~0x04F6)
    if (Opcode > 0x0FFF)
    {
        UE_LOG(LogWowPacket, Warning, TEXT("Ignoring out-of-range opcode 0x%04X (%d bytes)"), Opcode, Data.Num());
        return;
    }

    HandlerFunc* Found = Handlers.Find(Opcode);
    if (Found)
    {
        FPacketReader R(Data);
        (this->**Found)(R);
    }
    else
    {
        UE_LOG(LogWowPacket, Verbose, TEXT("Unhandled opcode 0x%04X (%s) — %d bytes"),
            Opcode, WowOpcode::GetName(Opcode), Data.Num());
    }

    // Notify subscribers (EventSystem wires this to fire WoW UI events)
    OnOpcodeReceived.Broadcast(Opcode);
}

// ── SMSG_LOGIN_VERIFY_WORLD ──────────────────────────────────────────────────
// uint32 mapId, float x, float y, float z, float orientation

void FWowPacketHandler::HandleLoginVerifyWorld(FPacketReader& R)
{
    uint32 MapId = R.ReadU32();
    float X = R.ReadFloat();
    float Y = R.ReadFloat();
    float Z = R.ReadFloat();
    float O = R.ReadFloat();

    UE_LOG(LogWowPacket, Log, TEXT("LOGIN_VERIFY_WORLD: map=%d pos=(%.1f, %.1f, %.1f) orient=%.2f"),
        MapId, X, Y, Z, O);

    OnLoginVerifyWorld.Broadcast(MapId, X, Y, Z, O);
}

// ── SMSG_UPDATE_OBJECT ───────────────────────────────────────────────────────
// uint32 blockCount, then blockCount update blocks

void FWowPacketHandler::HandleUpdateObject(FPacketReader& R)
{
    uint32 BlockCount = R.ReadU32();

    for (uint32 i = 0; i < BlockCount && R.Remaining() > 0; ++i)
    {
        ParseUpdateBlock(R);
    }

    UE_LOG(LogWowPacket, Log, TEXT("UPDATE_OBJECT: %d blocks (total entities: %d created, %d updated, %d destroyed, %d tracked)"),
        BlockCount, EntitiesCreated, EntitiesUpdated, EntitiesDestroyed, EntityManager.Num());
}

// ── SMSG_COMPRESSED_UPDATE_OBJECT ────────────────────────────────────────────
// uint32 decompressedSize, then zlib-compressed UPDATE_OBJECT payload

void FWowPacketHandler::HandleCompressedUpdateObject(FPacketReader& R)
{
    uint32 DecompSize = R.ReadU32();
    int32 CompSize = R.Remaining();

    if (CompSize <= 0 || DecompSize == 0 || DecompSize > 10 * 1024 * 1024)
    {
        UE_LOG(LogWowPacket, Warning, TEXT("COMPRESSED_UPDATE_OBJECT: invalid sizes (comp=%d, decomp=%u)"), CompSize, DecompSize);
        return;
    }

    TArray<uint8> Decompressed;
    Decompressed.SetNumUninitialized(DecompSize);

    // zlib decompress
    int32 UncompSize = static_cast<int32>(DecompSize);
    if (!FCompression::UncompressMemory(NAME_Zlib, Decompressed.GetData(), UncompSize,
            R.Data + R.Pos, CompSize))
    {
        UE_LOG(LogWowPacket, Warning, TEXT("COMPRESSED_UPDATE_OBJECT: zlib decompress failed"));
        return;
    }

    UE_LOG(LogWowPacket, Verbose, TEXT("COMPRESSED_UPDATE_OBJECT: decompressed %d → %d bytes"), CompSize, UncompSize);

    // Parse as regular UPDATE_OBJECT
    FPacketReader DecompReader(Decompressed);
    uint32 BlockCount = DecompReader.ReadU32();
    for (uint32 i = 0; i < BlockCount && DecompReader.Remaining() > 0; ++i)
    {
        ParseUpdateBlock(DecompReader);
    }

    UE_LOG(LogWowPacket, Log, TEXT("COMPRESSED_UPDATE_OBJECT: %d blocks (total: %d entities tracked)"),
        BlockCount, EntityManager.Num());
}

// ── Parse a single update block ──────────────────────────────────────────────

void FWowPacketHandler::ParseUpdateBlock(FPacketReader& R)
{
    uint8 Type = R.ReadU8();

    switch (Type)
    {
    case UpdateType::VALUES:
    {
        uint64 Guid = R.ReadPackedGuid();
        FWowEntity* Entity = EntityManager.Find(Guid);
        if (Entity)
        {
            ParseUpdateFields(R, *Entity);
            if (const uint32 UpdatedTypeMask = Entity->GetField(ObjectField::TYPE); UpdatedTypeMask != 0)
            {
                Entity = &EntityManager.PromoteToTyped(Guid, UpdatedTypeMask);
                Entity->Entry = Entity->GetField(ObjectField::ENTRY);
                Entity->Scale = Entity->GetFieldFloat(ObjectField::SCALE_X);
                if (Entity->Scale == 0.0f)
                {
                    Entity->Scale = 1.0f;
                }
            }

            // Check for player death
            if (Entity->Guid == EntityManager.LocalPlayerGuid && Entity->IsUnit())
            {
                int32 Health = Entity->GetHealth();
                if (Health == 0)
                {
                    UE_LOG(LogWowPacket, Warning, TEXT("Player has died! Health = 0"));
                    OnPlayerDeath.Broadcast();
                }
            }

            EntitiesUpdated++;
            EntityManager.OnEntityUpdated.Broadcast(*Entity);
        }
        else
        {
            UE_LOG(LogWowPacket, Warning, TEXT("VALUES update for unknown GUID %llu"), Guid);
            // Skip fields we can't parse without knowing the entity
            break;
        }
        break;
    }

    case UpdateType::MOVEMENT:
    {
        uint64 Guid = R.ReadPackedGuid();
        FWowEntity& Entity = EntityManager.GetOrCreate(Guid);
        // Movement block
        {
            uint16 Flags = R.ReadU16();
            if (Flags & UpdateFlag::LIVING)
            {
                ParseMovementInfo(R, Entity.Movement);
            }
            else if (Flags & UpdateFlag::STATIONARY_POSITION)
            {
                Entity.Movement.Position.X = R.ReadFloat();
                Entity.Movement.Position.Y = R.ReadFloat();
                Entity.Movement.Position.Z = R.ReadFloat();
                Entity.Movement.Orientation = R.ReadFloat();
            }
        }
        EntityManager.OnEntityUpdated.Broadcast(Entity);
        break;
    }

    case UpdateType::CREATE_OBJECT:
    case UpdateType::CREATE_OBJECT2:
    {
        uint64 Guid = R.ReadPackedGuid();
        uint8 ObjTypeId = R.ReadU8();

        FWowEntity& BaseEntity = EntityManager.GetOrCreate(Guid);
        BaseEntity.ObjectTypeId = ObjTypeId;

        // Movement/position block
        uint16 Flags = R.ReadU16();

        if (Flags & UpdateFlag::LIVING)
        {
            ParseMovementInfo(R, BaseEntity.Movement);
        }
        else if (Flags & UpdateFlag::STATIONARY_POSITION)
        {
            BaseEntity.Movement.Position.X = R.ReadFloat();
            BaseEntity.Movement.Position.Y = R.ReadFloat();
            BaseEntity.Movement.Position.Z = R.ReadFloat();
            BaseEntity.Movement.Orientation = R.ReadFloat();
        }

        if (Flags & UpdateFlag::LOWGUID)
        {
            R.ReadU32(); // lowGuid extra
        }

        if (Flags & UpdateFlag::HAS_TARGET)
        {
            R.ReadPackedGuid(); // target GUID
        }

        if (Flags & UpdateFlag::TRANSPORT)
        {
            R.ReadU32(); // transport timer
        }

        if (Flags & UpdateFlag::VEHICLE)
        {
            R.ReadU32(); // vehicle ID
            R.ReadFloat(); // vehicle aim adjustment
        }

        if (Flags & UpdateFlag::ROTATION)
        {
            R.ReadU64(); // packed rotation (gameobjects)
        }

        // Update fields
        ParseUpdateFields(R, BaseEntity);

        // Extract common fields
        BaseEntity.TypeMask = BaseEntity.GetField(ObjectField::TYPE);
        BaseEntity.Entry = BaseEntity.GetField(ObjectField::ENTRY);
        BaseEntity.Scale = BaseEntity.GetFieldFloat(ObjectField::SCALE_X);
        if (BaseEntity.Scale == 0.0f)
        {
            BaseEntity.Scale = 1.0f;
        }

        FWowEntity& Entity = EntityManager.PromoteToTyped(Guid, BaseEntity.TypeMask);
        Entity.ObjectTypeId = ObjTypeId;
        Entity.Entry = Entity.GetField(ObjectField::ENTRY);
        Entity.Scale = Entity.GetFieldFloat(ObjectField::SCALE_X);
        if (Entity.Scale == 0.0f)
        {
            Entity.Scale = 1.0f;
        }

        EntitiesCreated++;
        EntityManager.OnEntityCreated.Broadcast(Entity);

        UE_LOG(LogWowPacket, Verbose, TEXT("Created %s GUID=%llu type=%d entry=%u pos=(%.1f,%.1f,%.1f)"),
            Entity.GetEntityKindName(), Guid, ObjTypeId, Entity.Entry,
            Entity.Movement.Position.X, Entity.Movement.Position.Y, Entity.Movement.Position.Z);
        break;
    }

    case UpdateType::OUT_OF_RANGE_OBJECTS:
    {
        uint32 Count = R.ReadU32();
        for (uint32 i = 0; i < Count && R.Remaining() > 0; ++i)
        {
            uint64 Guid = R.ReadPackedGuid();
            EntityManager.Remove(Guid);
            EntitiesDestroyed++;
        }
        UE_LOG(LogWowPacket, Verbose, TEXT("OUT_OF_RANGE: %d objects removed"), Count);
        break;
    }

    case UpdateType::NEAR_OBJECTS:
    {
        uint32 Count = R.ReadU32();
        for (uint32 i = 0; i < Count && R.Remaining() > 0; ++i)
        {
            R.ReadPackedGuid(); // just acknowledge
        }
        break;
    }

    default:
        UE_LOG(LogWowPacket, Warning, TEXT("Unknown update block type: %d"), Type);
        break;
    }
}

// ── Parse movement info for LIVING entities ──────────────────────────────────

void FWowPacketHandler::ParseMovementInfo(FPacketReader& R, FWowMovementInfo& Out)
{
    Out.MoveFlags = R.ReadU32();
    Out.MoveFlags2 = R.ReadU16();
    R.ReadU32(); // timestamp

    Out.Position.X = R.ReadFloat();
    Out.Position.Y = R.ReadFloat();
    Out.Position.Z = R.ReadFloat();
    Out.Orientation = R.ReadFloat();

    // Transport
    if (Out.MoveFlags & 0x00000200) // MOVEMENTFLAG_ONTRANSPORT
    {
        Out.TransportGuid = R.ReadPackedGuid();
        Out.TransportOffset.X = R.ReadFloat();
        Out.TransportOffset.Y = R.ReadFloat();
        Out.TransportOffset.Z = R.ReadFloat();
        Out.TransportOrientation = R.ReadFloat();
        R.ReadU32(); // transport time
        R.ReadU8();  // transport seat

        if (Out.MoveFlags2 & 0x0200) // MOVEMENTFLAG2_INTERP_MOVEMENT
        {
            R.ReadU32(); // transport time 2
        }
    }

    // Swimming / Flying
    if (Out.MoveFlags & (0x00200000 | 0x02000000)) // SWIMMING | FLYING
    {
        R.ReadFloat(); // pitch
    }

    Out.FallTime = R.ReadU32();

    // Falling
    if (Out.MoveFlags & 0x00001000) // FALLING
    {
        R.ReadFloat(); // jump velocity
        R.ReadFloat(); // jump sin
        R.ReadFloat(); // jump cos
        R.ReadFloat(); // jump xy speed
    }

    // Spline elevation
    if (Out.MoveFlags & 0x04000000) // SPLINE_ELEVATION
    {
        R.ReadFloat();
    }

    // Speeds (9 speeds for living entities)
    Out.WalkSpeed = R.ReadFloat();
    Out.RunSpeed = R.ReadFloat();
    Out.RunBackSpeed = R.ReadFloat();
    Out.SwimSpeed = R.ReadFloat();
    Out.SwimBackSpeed = R.ReadFloat();
    Out.FlightSpeed = R.ReadFloat();
    Out.FlightBackSpeed = R.ReadFloat();
    Out.TurnRate = R.ReadFloat();
    Out.PitchRate = R.ReadFloat();

    // Spline data
    if (Out.MoveFlags & 0x08000000) // SPLINE_ENABLED
    {
        // Skip spline data — variable length, complex
        uint32 SplineFlags = R.ReadU32();
        if (SplineFlags & 0x00020000) // SPLINEFLAG_FINAL_ANGLE
        {
            R.ReadFloat();
        }
        else if (SplineFlags & 0x00040000) // SPLINEFLAG_FINAL_TARGET
        {
            R.ReadU64();
        }
        else if (SplineFlags & 0x00080000) // SPLINEFLAG_FINAL_POINT
        {
            R.ReadFloat();
            R.ReadFloat();
            R.ReadFloat();
        }

        R.ReadU32(); // spline time passed
        R.ReadU32(); // spline duration
        R.ReadU32(); // spline id

        R.ReadFloat(); // spline duration multiplier
        R.ReadFloat(); // spline duration multiplier next

        R.ReadU32(); // spline vertical acceleration

        R.ReadU32(); // spline time started

        uint32 SplineCount = R.ReadU32();
        for (uint32 i = 0; i < SplineCount; ++i)
        {
            R.ReadFloat(); R.ReadFloat(); R.ReadFloat(); // point
        }

        R.ReadU8(); // spline mode

        R.ReadFloat(); R.ReadFloat(); R.ReadFloat(); // endpoint
    }
}

// ── Parse bitmask-based update fields ────────────────────────────────────────

void FWowPacketHandler::ParseUpdateFields(FPacketReader& R, FWowEntity& Entity)
{
    // The update fields block starts with a count of uint32 bitmask blocks
    uint8 BlockCount = R.ReadU8();

    if (BlockCount == 0) return;

    // Read the bitmask
    TArray<uint32> Mask;
    Mask.SetNum(BlockCount);
    for (int32 i = 0; i < BlockCount; ++i)
    {
        Mask[i] = R.ReadU32();
    }

    bool bInventoryFieldUpdated = false;

    // Each set bit in the mask means a uint32 value follows
    for (int32 Block = 0; Block < BlockCount; ++Block)
    {
        for (int32 Bit = 0; Bit < 32; ++Bit)
        {
            if (Mask[Block] & (1u << Bit))
            {
                uint16 FieldIndex = static_cast<uint16>(Block * 32 + Bit);
                uint32 Value = R.ReadU32();
                Entity.SetField(FieldIndex, Value);

                // Check if this is a player inventory field update
                if (Entity.IsPlayer() &&
                    ((FieldIndex >= PlayerField::INV_SLOT_HEAD && FieldIndex <= PlayerField::PACK_SLOT_END) ||
                     (FieldIndex >= PlayerField::PACK_SLOT_1 && FieldIndex <= PlayerField::PACK_SLOT_LAST + 1)))
                {
                    bInventoryFieldUpdated = true;
                }
            }
        }
    }

    // Fire inventory update event if player inventory was updated
    if (bInventoryFieldUpdated)
    {
        OnPlayerInventoryUpdate.Broadcast();
    }
}

// ── SMSG_DESTROY_OBJECT ──────────────────────────────────────────────────────

void FWowPacketHandler::HandleDestroyObject(FPacketReader& R)
{
    uint64 Guid = R.ReadU64();
    R.ReadU8(); // onDeath flag

    EntityManager.Remove(Guid);
    EntitiesDestroyed++;

    UE_LOG(LogWowPacket, Verbose, TEXT("DESTROY_OBJECT: GUID=%llu (total tracked: %d)"), Guid, EntityManager.Num());
}

// ── Movement packets ─────────────────────────────────────────────────────────

void FWowPacketHandler::HandleMovement(FPacketReader& R)
{
    uint64 Guid = R.ReadPackedGuid();
    FWowEntity* Entity = EntityManager.Find(Guid);
    if (!Entity) return;

    FWowMovementInfo Info;
    Info.MoveFlags = R.ReadU32();
    Info.MoveFlags2 = R.ReadU16();
    R.ReadU32(); // timestamp

    Info.Position.X = R.ReadFloat();
    Info.Position.Y = R.ReadFloat();
    Info.Position.Z = R.ReadFloat();
    Info.Orientation = R.ReadFloat();

    // Transport
    if (Info.MoveFlags & 0x00000200)
    {
        Info.TransportGuid = R.ReadPackedGuid();
        Info.TransportOffset.X = R.ReadFloat();
        Info.TransportOffset.Y = R.ReadFloat();
        Info.TransportOffset.Z = R.ReadFloat();
        Info.TransportOrientation = R.ReadFloat();
        R.ReadU32(); // time
        R.ReadU8();  // seat
    }

    if (Info.MoveFlags & (0x00200000 | 0x02000000))
    {
        R.ReadFloat(); // pitch
    }

    Info.FallTime = R.ReadU32();

    if (Info.MoveFlags & 0x00001000)
    {
        R.ReadFloat(); R.ReadFloat(); R.ReadFloat(); R.ReadFloat();
    }

    if (Info.MoveFlags & 0x04000000)
    {
        R.ReadFloat();
    }

    Entity->Movement = Info;
    EntityManager.OnEntityUpdated.Broadcast(*Entity);
}

// ── SMSG_MESSAGECHAT ─────────────────────────────────────────────────────────

void FWowPacketHandler::HandleMessageChat(FPacketReader& R)
{
    uint8 Type = R.ReadU8();
    uint32 Language = R.ReadU32();
    uint64 SenderGuid = R.ReadU64();
    R.ReadU32(); // flags

    // Channel name for channel messages
    FString Channel;
    if (Type == 17) // CHAT_MSG_CHANNEL
    {
        Channel = R.ReadCString();
    }

    uint64 TargetGuid = R.ReadU64();
    uint32 MsgLen = R.ReadU32();
    FString Message = R.ReadCString();
    uint8 ChatTag = R.ReadU8();

    // Get sender name from cache
    FString SenderName;
    if (SenderGuid != 0)
    {
        FString* CachedName = PlayerNameCache.Find(SenderGuid);
        if (CachedName)
        {
            SenderName = *CachedName;
        }
        else
        {
            SenderName = FString::Printf(TEXT("Player-%llu"), SenderGuid);
        }
    }

    UE_LOG(LogWowPacket, Log, TEXT("CHAT type=%d lang=%d sender=%s channel=%s: %s"),
           Type, Language, *SenderName, *Channel, *Message);
    OnChatMessage.Broadcast(Type, Language, SenderGuid, SenderName, Message, Channel);
}

// ── SMSG_INITIAL_SPELLS ──────────────────────────────────────────────────────

void FWowPacketHandler::HandleInitialSpells(FPacketReader& R)
{
    R.ReadU8(); // talent spec
    uint16 SpellCount = R.ReadU16();

    KnownSpells.Empty(SpellCount);
    for (int32 i = 0; i < SpellCount; ++i)
    {
        uint32 SpellId = R.ReadU32();
        R.ReadU16(); // unknown (slot?)
        KnownSpells.Add(SpellId);
    }

    uint16 CooldownCount = R.ReadU16();
    for (int32 i = 0; i < CooldownCount; ++i)
    {
        R.ReadU32(); // spell ID
        R.ReadU16(); // item ID
        R.ReadU16(); // spell category
        R.ReadU32(); // cooldown
        R.ReadU32(); // category cooldown
    }

    UE_LOG(LogWowPacket, Log, TEXT("INITIAL_SPELLS: %d spells stored, %d cooldowns"), SpellCount, CooldownCount);
}

// ── SMSG_ACTION_BUTTONS ──────────────────────────────────────────────────────

void FWowPacketHandler::HandleActionButtons(FPacketReader& R)
{
    // 144 action buttons × 4 bytes each = 576 bytes
    int32 ButtonCount = FMath::Min(R.Remaining() / 4, 144);

    ActionButtons.Empty(ButtonCount);
    ActionButtons.AddZeroed(ButtonCount);

    int32 NonEmpty = 0;
    for (int32 i = 0; i < ButtonCount; ++i)
    {
        uint32 PackedAction = R.ReadU32();
        ActionButtons[i] = PackedAction;
        if (PackedAction != 0) NonEmpty++;
    }

    UE_LOG(LogWowPacket, Log, TEXT("ACTION_BUTTONS: %d/%d slots assigned"), NonEmpty, ButtonCount);
}

// ── SMSG_TIME_SYNC_REQ ──────────────────────────────────────────────────────

void FWowPacketHandler::HandleTimeSyncReq(FPacketReader& R)
{
    uint32 Counter = R.ReadU32();
    UE_LOG(LogWowPacket, Verbose, TEXT("TIME_SYNC_REQ: counter=%u"), Counter);

    // Respond with CMSG_TIME_SYNC_RESP: uint32 counter + uint32 clientTicks
    TArray<uint8> Resp;
    Resp.SetNumUninitialized(8);
    FMemory::Memcpy(Resp.GetData(), &Counter, 4);
    uint32 ClientTicks = FPlatformTime::Cycles(); // monotonic tick
    FMemory::Memcpy(Resp.GetData() + 4, &ClientTicks, 4);

    OnSendPacket.ExecuteIfBound(WowOpcode::CMSG_TIME_SYNC_RESP, Resp);
}

// ── SMSG_SPELL_START ─────────────────────────────────────────────────────
// packed guid caster, packed guid caster again, uint8 cast counter, uint32 spell id, uint32 cast flags, int32 cast time, (targets)

void FWowPacketHandler::HandleSpellStart(FPacketReader& R)
{
    if (!R.CanRead(18)) // minimum: 2 packed guids (2 bytes each) + cast counter + spell id + cast flags + cast time
    {
        UE_LOG(LogWowPacket, Warning, TEXT("SPELL_START: packet too short"));
        return;
    }

    uint64 CasterGuid1 = R.ReadPackedGuid(); // Item or caster guid
    uint64 CasterGuid = R.ReadPackedGuid();  // Actual caster guid
    uint8 CastCounter = R.ReadU8();
    uint32 SpellId = R.ReadU32();
    uint32 CastFlags = R.ReadU32();
    int32 CastTime = static_cast<int32>(R.ReadU32());

    UE_LOG(LogWowPacket, Log, TEXT("SPELL_START: caster=%llu spell=%u castTime=%dms"),
        CasterGuid, SpellId, CastTime);

    OnSpellStart.Broadcast(CasterGuid, SpellId, CastFlags, CastTime);

    // Skip remaining data (targets and additional cast flag data)
}

// ── SMSG_SPELL_GO ───────────────────────────────────────────────────────
// packed guid caster, packed guid caster again, uint8 cast counter, uint32 spell id, uint32 cast flags, uint32 timestamp,
// uint8 hit count, packed guids..., uint8 miss count, packed guids + miss reasons...

void FWowPacketHandler::HandleSpellGo(FPacketReader& R)
{
    if (!R.CanRead(20)) // minimum: 2 packed guids + cast counter + spell id + cast flags + timestamp + hit count + miss count
    {
        UE_LOG(LogWowPacket, Warning, TEXT("SPELL_GO: packet too short"));
        return;
    }

    uint64 CasterGuid1 = R.ReadPackedGuid(); // Item or caster guid
    uint64 CasterGuid = R.ReadPackedGuid();  // Actual caster guid
    uint8 CastCounter = R.ReadU8();
    uint32 SpellId = R.ReadU32();
    uint32 CastFlags = R.ReadU32();
    uint32 Timestamp = R.ReadU32();

    // Hit targets
    if (!R.CanRead(1)) return;
    uint8 HitCount = R.ReadU8();
    for (uint32 i = 0; i < HitCount; ++i)
    {
        if (R.Remaining() == 0) break;
        R.ReadPackedGuid(); // hit target guid
    }

    // Miss targets
    if (!R.CanRead(1)) return;
    uint8 MissCount = R.ReadU8();
    for (uint32 i = 0; i < MissCount; ++i)
    {
        if (!R.CanRead(2)) break; // need at least guid + miss reason
        R.ReadPackedGuid(); // miss target guid
        uint8 MissReason = R.ReadU8();
        if (MissReason == 11) // SPELL_MISS_REFLECT
        {
            if (R.CanRead(1))
                R.ReadU8(); // reflect result
        }
    }

    UE_LOG(LogWowPacket, Log, TEXT("SPELL_GO: caster=%llu spell=%u hits=%d misses=%d"),
        CasterGuid, SpellId, HitCount, MissCount);

    OnSpellGo.Broadcast(CasterGuid, SpellId, CastFlags);

    // Skip remaining data (targets and additional cast flag data)
}

// ── SMSG_SPELL_FAILURE ──────────────────────────────────────────────────
// packed guid caster, uint8 cast counter, uint32 spell id, uint8 failure reason

void FWowPacketHandler::HandleSpellFailure(FPacketReader& R)
{
    if (!R.CanRead(7)) // minimum: packed guid (1) + cast counter + spell id + failure reason
    {
        UE_LOG(LogWowPacket, Warning, TEXT("SPELL_FAILURE: packet too short"));
        return;
    }

    uint64 CasterGuid = R.ReadPackedGuid();
    uint8 CastCounter = R.ReadU8();
    uint32 SpellId = R.ReadU32();
    uint8 FailureReason = R.ReadU8();

    UE_LOG(LogWowPacket, Log, TEXT("SPELL_FAILURE: caster=%llu spell=%u reason=%u"),
        CasterGuid, SpellId, FailureReason);

    OnSpellFailure.Broadcast(CasterGuid, SpellId, FailureReason);
}

// ── SMSG_ATTACKERSTATEUPDATE ────────────────────────────────────────────
// uint32 hit_info, packed guid attacker, packed guid target, uint32 damage, uint8 damage_school_count
// for each school: uint32 damage, uint32 absorbed

void FWowPacketHandler::HandleAttackerStateUpdate(FPacketReader& R)
{
    if (!R.CanRead(8)) // minimum: hit_info + 2 packed guids (at least 1 byte each)
    {
        UE_LOG(LogWowPacket, Warning, TEXT("ATTACKERSTATEUPDATE: packet too short"));
        return;
    }

    uint32 HitInfo = R.ReadU32();
    uint64 AttackerGuid = R.ReadPackedGuid();
    uint64 TargetGuid = R.ReadPackedGuid();

    if (!R.CanRead(5)) // damage + school count
    {
        UE_LOG(LogWowPacket, Warning, TEXT("ATTACKERSTATEUPDATE: packet truncated"));
        return;
    }

    uint32 TotalDamage = R.ReadU32();
    uint8 DamageSchoolCount = R.ReadU8();

    // Parse damage by school (for now just track total damage)
    for (uint8 i = 0; i < DamageSchoolCount && R.CanRead(8); ++i)
    {
        uint32 SchoolDamage = R.ReadU32();
        uint32 SchoolAbsorbed = R.ReadU32();
        // We could track school-specific damage but for basic combat text, total damage is enough
    }

    UE_LOG(LogWowPacket, Log, TEXT("ATTACKERSTATEUPDATE: attacker=%llu target=%llu damage=%u hitInfo=0x%08X"),
        AttackerGuid, TargetGuid, TotalDamage, HitInfo);

    OnAttackerStateUpdate.Broadcast(AttackerGuid, TargetGuid, HitInfo, TotalDamage);
}

// ── SMSG_AURA_UPDATE ────────────────────────────────────────────────────
// packed guid target, uint8 slot, [if slot != 0xFF: uint32 spell id, uint8 flags, uint8 level, uint8 charges, possible packed guid caster]

void FWowPacketHandler::HandleAuraUpdate(FPacketReader& R)
{
    if (!R.CanRead(2)) // minimum: packed guid + slot
    {
        UE_LOG(LogWowPacket, Warning, TEXT("AURA_UPDATE: packet too short"));
        return;
    }

    uint64 TargetGuid = R.ReadPackedGuid();

    FWowEntity* Entity = EntityManager.Find(TargetGuid);
    if (!Entity)
    {
        UE_LOG(LogWowPacket, Warning, TEXT("AURA_UPDATE: unknown entity %llu"), TargetGuid);
        return;
    }

    if (!R.CanRead(1)) return;
    uint8 Slot = R.ReadU8();

    if (Slot == 0xFF)
    {
        // End of aura updates
        return;
    }

    if (Slot >= Entity->Auras.Num())
    {
        UE_LOG(LogWowPacket, Warning, TEXT("AURA_UPDATE: invalid slot %d"), Slot);
        return;
    }

    if (!R.CanRead(4)) return;
    uint32 SpellId = R.ReadU32();

    if (SpellId == 0)
    {
        // Remove aura
        Entity->Auras[Slot].bActive = false;
        Entity->Auras[Slot].SpellId = 0;
        UE_LOG(LogWowPacket, Verbose, TEXT("AURA_UPDATE: target=%llu slot=%d removed"), TargetGuid, Slot);
        return;
    }

    // Add/update aura
    if (!R.CanRead(3)) return;
    uint8 Flags = R.ReadU8();
    uint8 Level = R.ReadU8();
    uint8 Charges = R.ReadU8();

    FAuraInfo& Aura = Entity->Auras[Slot];
    Aura.SpellId = SpellId;
    Aura.Flags = Flags;
    Aura.Level = Level;
    Aura.Charges = Charges;
    Aura.bActive = true;

    // Check if caster GUID is present (depends on flags)
    const uint8 AFLAG_CASTER = 0x08;
    if (Flags & AFLAG_CASTER)
    {
        if (R.Remaining() > 0)
        {
            Aura.CasterGuid = R.ReadPackedGuid();
        }
    }

    UE_LOG(LogWowPacket, Log, TEXT("AURA_UPDATE: target=%llu slot=%d spell=%u"),
        TargetGuid, Slot, SpellId);
}

// ── SMSG_POWER_UPDATE ───────────────────────────────────────────────────
// packed guid target, uint8 power type, uint32 power value

void FWowPacketHandler::HandlePowerUpdate(FPacketReader& R)
{
    if (!R.CanRead(6)) // minimum: packed guid + power type + power value
    {
        UE_LOG(LogWowPacket, Warning, TEXT("POWER_UPDATE: packet too short"));
        return;
    }

    uint64 TargetGuid = R.ReadPackedGuid();
    uint8 PowerType = R.ReadU8();
    uint32 PowerValue = R.ReadU32();

    FWowEntity* Entity = EntityManager.Find(TargetGuid);
    if (Entity)
    {
        // Update power field in entity based on power type
        if (PowerType < 7) // Valid power types: 0=mana, 1=rage, 2=focus, 3=energy, 4=happiness, 5=runes, 6=runic power
        {
            // WoW power fields start at UNIT_FIELD_POWER1 + power_type
            uint16 PowerFieldIndex = UnitField::POWER1 + PowerType;
            Entity->SetField(PowerFieldIndex, PowerValue);
        }
    }

    UE_LOG(LogWowPacket, Log, TEXT("POWER_UPDATE: target=%llu type=%d value=%u"),
        TargetGuid, PowerType, PowerValue);
}

// ── SMSG_MONSTER_MOVE ───────────────────────────────────────────────────
// packed guid target, uint8 new_in_31, 3 floats position, uint32 timestamp, uint8 move_type,
// [if move_type == 0: uint32 spline_flags, uint32 duration, uint32 point_count, points...]

void FWowPacketHandler::HandleMonsterMove(FPacketReader& R)
{
    if (!R.CanRead(18)) // minimum: packed guid + new flag + position + timestamp + move type
    {
        UE_LOG(LogWowPacket, Warning, TEXT("MONSTER_MOVE: packet too short"));
        return;
    }

    uint64 TargetGuid = R.ReadPackedGuid();
    uint8 NewIn31 = R.ReadU8(); // new in 3.1 - usually 0
    float PosX = R.ReadFloat();
    float PosY = R.ReadFloat();
    float PosZ = R.ReadFloat();
    uint32 Timestamp = R.ReadU32();
    uint8 MoveType = R.ReadU8();

    FWowEntity* Entity = EntityManager.Find(TargetGuid);
    if (!Entity)
    {
        UE_LOG(LogWowPacket, Warning, TEXT("MONSTER_MOVE: entity with GUID %llu not found"), TargetGuid);
        return;
    }

    // Store current position as the starting point for spline movement
    Entity->Movement.SplineStartPosition = Entity->Movement.Position;

    // Update entity position to the starting position from packet
    Entity->Movement.Position.X = PosX;
    Entity->Movement.Position.Y = PosY;
    Entity->Movement.Position.Z = PosZ;
    Entity->Movement.SplineMoveType = MoveType;

    uint32 PointCount = 0;

    if (MoveType == 1) // Stop movement
    {
        // Just update position and clear any active spline
        Entity->Movement.bHasActiveSpline = false;
        Entity->Movement.SplineWaypoints.Empty();
        UE_LOG(LogWowPacket, Log, TEXT("MONSTER_MOVE: STOP guid=%llu pos=(%.1f,%.1f,%.1f)"),
            TargetGuid, PosX, PosY, PosZ);
    }
    else if (MoveType == 2) // Facing point
    {
        if (R.CanRead(12))
        {
            float FaceX = R.ReadFloat();
            float FaceY = R.ReadFloat();
            float FaceZ = R.ReadFloat();
            // Calculate orientation to face the point
            FVector FaceDir = FVector(FaceX - PosX, FaceY - PosY, 0.0f);
            if (!FaceDir.IsNearlyZero())
            {
                Entity->Movement.Orientation = FMath::Atan2(FaceDir.Y, FaceDir.X);
            }
            UE_LOG(LogWowPacket, Log, TEXT("MONSTER_MOVE: FACE_POINT guid=%llu face=(%.1f,%.1f,%.1f)"),
                TargetGuid, FaceX, FaceY, FaceZ);
        }
        Entity->Movement.bHasActiveSpline = false;
        Entity->Movement.SplineWaypoints.Empty();
    }
    else if (MoveType == 3) // Facing angle
    {
        if (R.CanRead(4))
        {
            float FaceAngle = R.ReadFloat();
            Entity->Movement.Orientation = FaceAngle;
            UE_LOG(LogWowPacket, Log, TEXT("MONSTER_MOVE: FACE_ANGLE guid=%llu angle=%.3f"),
                TargetGuid, FaceAngle);
        }
        Entity->Movement.bHasActiveSpline = false;
        Entity->Movement.SplineWaypoints.Empty();
    }
    else if (MoveType == 4) // Facing target
    {
        if (R.CanRead(8))
        {
            uint64 FaceTarget = R.ReadU64();
            UE_LOG(LogWowPacket, Log, TEXT("MONSTER_MOVE: FACE_TARGET guid=%llu target=%llu"),
                TargetGuid, FaceTarget);
        }
        Entity->Movement.bHasActiveSpline = false;
        Entity->Movement.SplineWaypoints.Empty();
    }
    else if (MoveType == 0) // Normal movement with spline
    {
        if (!R.CanRead(12))
        {
            UE_LOG(LogWowPacket, Warning, TEXT("MONSTER_MOVE: insufficient data for spline"));
            return;
        }

        uint32 SplineFlags = R.ReadU32();
        uint32 Duration = R.ReadU32();
        PointCount = R.ReadU32();

        Entity->Movement.SplineFlags = SplineFlags;
        Entity->Movement.SplineDuration = Duration;
        Entity->Movement.SplineElapsed = 0.0f;
        Entity->Movement.SplineWaypoints.Empty();
        Entity->Movement.bHasActiveSpline = (PointCount > 0 && Duration > 0);

        // Read waypoints
        for (uint32 i = 0; i < PointCount && R.CanRead(12); ++i)
        {
            float WPX = R.ReadFloat();
            float WPY = R.ReadFloat();
            float WPZ = R.ReadFloat();
            Entity->Movement.SplineWaypoints.Add(FWowSplineWaypoint(FVector(WPX, WPY, WPZ)));
        }

        UE_LOG(LogWowPacket, Log, TEXT("MONSTER_MOVE: SPLINE guid=%llu pos=(%.1f,%.1f,%.1f) duration=%ums points=%u"),
            TargetGuid, PosX, PosY, PosZ, Duration, PointCount);
    }
    else
    {
        UE_LOG(LogWowPacket, Warning, TEXT("MONSTER_MOVE: unknown move type %u for guid %llu"), MoveType, TargetGuid);
        Entity->Movement.bHasActiveSpline = false;
        Entity->Movement.SplineWaypoints.Empty();
    }

    EntityManager.OnEntityUpdated.Broadcast(*Entity);
}

// ── SMSG_INVENTORY_CHANGE_FAILURE ─────────────────────────────────────────
// uint8 error code

void FWowPacketHandler::HandleInventoryChangeFailure(FPacketReader& R)
{
    if (!R.CanRead(1)) return;

    uint8 ErrorCode = R.ReadU8();

    const TCHAR* ErrorMsg = TEXT("Unknown error");
    switch (ErrorCode)
    {
    case 0:  ErrorMsg = TEXT("CANT_EQUIP_LEVEL_I"); break;
    case 1:  ErrorMsg = TEXT("CANT_EQUIP_SKILL"); break;
    case 2:  ErrorMsg = TEXT("ITEM_DOESNT_GO_TO_SLOT"); break;
    case 3:  ErrorMsg = TEXT("BAG_FULL"); break;
    case 4:  ErrorMsg = TEXT("NONEMPTY_BAG_OVER_OTHER_BAG"); break;
    case 5:  ErrorMsg = TEXT("CANT_TRADE_EQUIP_BAGS"); break;
    case 6:  ErrorMsg = TEXT("ONLY_AMMO_CAN_GO_HERE"); break;
    case 7:  ErrorMsg = TEXT("NO_REQUIRED_PROFICIENCY"); break;
    case 8:  ErrorMsg = TEXT("NO_EQUIPMENT_SLOT_AVAILABLE"); break;
    case 9:  ErrorMsg = TEXT("YOU_CAN_NEVER_USE_THAT_ITEM"); break;
    case 10: ErrorMsg = TEXT("YOU_CAN_NEVER_USE_THAT_ITEM2"); break;
    case 11: ErrorMsg = TEXT("NO_EQUIPMENT_SLOT_AVAILABLE2"); break;
    case 12: ErrorMsg = TEXT("CANT_EQUIP_WITH_TWOHANDED"); break;
    case 13: ErrorMsg = TEXT("CANT_DUAL_WIELD"); break;
    case 14: ErrorMsg = TEXT("ITEM_DOESNT_GO_INTO_BAG"); break;
    case 15: ErrorMsg = TEXT("ITEM_DOESNT_GO_INTO_BAG2"); break;
    case 16: ErrorMsg = TEXT("CANT_CARRY_MORE_OF_THIS"); break;
    case 17: ErrorMsg = TEXT("NO_EQUIPMENT_SLOT_AVAILABLE3"); break;
    case 18: ErrorMsg = TEXT("ITEM_CANT_STACK"); break;
    case 19: ErrorMsg = TEXT("ITEM_CANT_BE_EQUIPPED"); break;
    case 20: ErrorMsg = TEXT("ITEMS_CANT_BE_SWAPPED"); break;
    case 21: ErrorMsg = TEXT("SLOT_IS_EMPTY"); break;
    case 22: ErrorMsg = TEXT("ITEM_NOT_FOUND"); break;
    case 23: ErrorMsg = TEXT("CANT_DROP_SOULBOUND"); break;
    case 24: ErrorMsg = TEXT("OUT_OF_RANGE"); break;
    case 25: ErrorMsg = TEXT("TRIED_TO_SPLIT_MORE_THAN_COUNT"); break;
    case 26: ErrorMsg = TEXT("COULDNT_SPLIT_ITEMS"); break;
    case 27: ErrorMsg = TEXT("MISSING_REAGENT"); break;
    case 28: ErrorMsg = TEXT("NOT_ENOUGH_MONEY"); break;
    case 29: ErrorMsg = TEXT("NOT_A_BAG"); break;
    case 30: ErrorMsg = TEXT("CAN_ONLY_DO_WITH_EMPTY_BAGS"); break;
    case 31: ErrorMsg = TEXT("DONT_OWN_THAT_ITEM"); break;
    case 32: ErrorMsg = TEXT("CAN_EQUIP_ONLY1_QUIVER"); break;
    case 33: ErrorMsg = TEXT("MUST_PURCHASE_THAT_BAG_SLOT"); break;
    case 34: ErrorMsg = TEXT("TOO_FAR_AWAY_FROM_BANK"); break;
    case 35: ErrorMsg = TEXT("ITEM_LOCKED"); break;
    case 36: ErrorMsg = TEXT("YOU_ARE_STUNNED"); break;
    case 37: ErrorMsg = TEXT("YOU_ARE_DEAD"); break;
    case 38: ErrorMsg = TEXT("CANT_DO_RIGHT_NOW"); break;
    case 39: ErrorMsg = TEXT("INT_BAG_ERROR"); break;
    case 40: ErrorMsg = TEXT("CAN_EQUIP_ONLY1_BOLT"); break;
    case 41: ErrorMsg = TEXT("CAN_EQUIP_ONLY1_AMMOPOUCH"); break;
    case 42: ErrorMsg = TEXT("STACKABLE_CANT_BE_WRAPPED"); break;
    case 43: ErrorMsg = TEXT("EQUIPPED_CANT_BE_WRAPPED"); break;
    case 44: ErrorMsg = TEXT("WRAPPED_CANT_BE_WRAPPED"); break;
    case 45: ErrorMsg = TEXT("BOUND_CANT_BE_WRAPPED"); break;
    case 46: ErrorMsg = TEXT("UNIQUE_CANT_BE_WRAPPED"); break;
    case 47: ErrorMsg = TEXT("BAGS_CANT_BE_WRAPPED"); break;
    case 48: ErrorMsg = TEXT("ALREADY_LOOTED"); break;
    case 49: ErrorMsg = TEXT("INVENTORY_FULL"); break;
    case 50: ErrorMsg = TEXT("BANK_FULL"); break;
    case 51: ErrorMsg = TEXT("ITEM_IS_CURRENTLY_SOLD_OUT"); break;
    case 52: ErrorMsg = TEXT("BAG_FULL3"); break;
    case 53: ErrorMsg = TEXT("ITEM_NOT_FOUND2"); break;
    case 54: ErrorMsg = TEXT("ITEM_CANT_STACK2"); break;
    case 55: ErrorMsg = TEXT("BAG_FULL4"); break;
    case 56: ErrorMsg = TEXT("ITEM_SOLD_OUT"); break;
    case 57: ErrorMsg = TEXT("OBJECT_IS_BUSY"); break;
    case 58: ErrorMsg = TEXT("NONE"); break;
    case 59: ErrorMsg = TEXT("NOT_IN_COMBAT"); break;
    case 60: ErrorMsg = TEXT("NOT_WHILE_DISARMED"); break;
    case 61: ErrorMsg = TEXT("BAG_FULL6"); break;
    case 62: ErrorMsg = TEXT("CANT_EQUIP_RANK"); break;
    case 63: ErrorMsg = TEXT("CANT_EQUIP_REPUTATION"); break;
    case 64: ErrorMsg = TEXT("TOO_MANY_SPECIAL_BAGS"); break;
    case 65: ErrorMsg = TEXT("LOOT_CANT_LOOT_THAT_NOW"); break;
    }

    UE_LOG(LogWowPacket, Warning, TEXT("INVENTORY_CHANGE_FAILURE: error=%d (%s)"), ErrorCode, ErrorMsg);
}

// ── SMSG_LOOT_RESPONSE ──────────────────────────────────────────────────
// packed guid loot_guid, uint8 loot_type, uint32 gold, uint8 item_count, then for each item:
// uint8 index, uint32 item_id, uint32 count, uint32 display_id, uint8 quality, ...

void FWowPacketHandler::HandleLootResponse(FPacketReader& R)
{
    if (!R.CanRead(2)) return; // minimum: packed guid + loot type

    uint64 LootGuid = R.ReadPackedGuid();

    if (!R.CanRead(5)) return; // loot type + gold + item count
    uint8 LootType = R.ReadU8();
    uint32 Gold = R.ReadU32();
    uint8 ItemCount = R.ReadU8();

    TArray<FWowLootItem> Items;
    Items.Reserve(ItemCount);

    for (int32 i = 0; i < ItemCount; ++i)
    {
        if (!R.CanRead(22)) break; // index + item_id + count + display_id + random_suffix + random_property + slot_type

        FWowLootItem Item;
        Item.Index = R.ReadU8();
        Item.ItemId = R.ReadU32();
        Item.Count = R.ReadU32();
        Item.DisplayId = R.ReadU32();
        Item.RandomSuffix = R.ReadU32();
        Item.RandomProperty = R.ReadU32();
        Item.SlotType = R.ReadU8();
        Item.bLooted = false;

        Items.Add(Item);
    }

    UE_LOG(LogWowPacket, Log, TEXT("LOOT_RESPONSE: guid=%llu type=%d gold=%u items=%d"),
           LootGuid, LootType, Gold, ItemCount);

    OnLootOpened.Broadcast(LootGuid, LootType, Gold, Items);
}

// ── SMSG_LOOT_RELEASE_RESPONSE ──────────────────────────────────────────────
// packed guid loot_guid, uint8 unknown

void FWowPacketHandler::HandleLootReleaseResponse(FPacketReader& R)
{
    if (!R.CanRead(2)) return; // minimum: packed guid + unknown

    uint64 LootGuid = R.ReadPackedGuid();

    if (!R.CanRead(1)) return;
    uint8 Unknown = R.ReadU8();

    UE_LOG(LogWowPacket, Log, TEXT("LOOT_RELEASE_RESPONSE: guid=%llu (loot window closed)"), LootGuid);

    OnLootClosed.Broadcast();
}

// ── SMSG_ITEM_PUSH_RESULT ──────────────────────────────────────────────────
// packed guid target_guid, uint8 created (0=existing item, 1=new), uint8 created2,
// uint8 bag_slot, uint32 item_slot, uint32 item_id, uint32 random_property_id,
// uint32 random_suffix, uint32 count, uint32 charges_count, uint8 charges1, uint8 charges2,
// uint8 charges3, uint8 charges4, uint8 charges5, uint32 duration, (optional item bonuses)

void FWowPacketHandler::HandleItemPushResult(FPacketReader& R)
{
    if (!R.CanRead(2)) return; // minimum: packed guid + created flag

    uint64 TargetGuid = R.ReadPackedGuid();

    if (!R.CanRead(18)) return; // created + created2 + bag + slot + item_id + other fields
    uint8 Created = R.ReadU8();
    uint8 Created2 = R.ReadU8();
    uint8 BagSlot = R.ReadU8();
    uint32 ItemSlot = R.ReadU32();
    uint32 ItemId = R.ReadU32();
    uint32 RandomPropertyId = R.ReadU32();

    if (!R.CanRead(8)) return; // random_suffix + count
    uint32 RandomSuffix = R.ReadU32();
    uint32 Count = R.ReadU32();

    // Skip remaining fields for now (charges, duration, etc.)

    const TCHAR* CreatedMsg = (Created == 1) ? TEXT("new") : TEXT("existing");
    UE_LOG(LogWowPacket, Log, TEXT("ITEM_PUSH_RESULT: target=%llu %s item=%u count=%u bag=%d slot=%u"),
           TargetGuid, CreatedMsg, ItemId, Count, BagSlot, ItemSlot);
}

// ── SMSG_LIST_INVENTORY ──────────────────────────────────────────────────────
// packed guid vendor_guid, uint8 item_count, for each item: uint32 slot + uint32 itemid + uint32 displayid + uint32 maxcount + uint32 price + uint32 maxdurability + uint32 buycount + uint32 extendedcost

void FWowPacketHandler::HandleListInventory(FPacketReader& R)
{
    if (!R.CanRead(2)) return; // minimum: packed guid + item count

    uint64 VendorGuid = R.ReadPackedGuid();

    if (!R.CanRead(1)) return;
    uint8 ItemCount = R.ReadU8();

    TArray<FWowVendorItem> Items;
    Items.Reserve(ItemCount);

    for (int32 i = 0; i < ItemCount; ++i)
    {
        if (!R.CanRead(32)) break; // slot + itemid + displayid + maxcount + price + maxdurability + buycount + extendedcost

        FWowVendorItem Item;
        Item.Slot = R.ReadU32();
        Item.ItemId = R.ReadU32();
        Item.DisplayId = R.ReadU32();
        Item.MaxCount = R.ReadU32();
        Item.Price = R.ReadU32();
        Item.MaxDurability = R.ReadU32();
        Item.BuyCount = R.ReadU32();
        Item.ExtendedCost = R.ReadU32();

        Items.Add(Item);
    }

    UE_LOG(LogWowPacket, Log, TEXT("LIST_INVENTORY: vendor=%llu items=%d"), VendorGuid, ItemCount);

    OnVendorOpened.Broadcast(VendorGuid, Items);
}

// ── ──────────────────────────────────────────────────────────────────────────
// ── Social / Guild / Friends System Handlers ────────────────────────────────
// ── ──────────────────────────────────────────────────────────────────────────

// ── SMSG_FRIEND_LIST ─────────────────────────────────────────────────────────
// uint8 count, for each friend: uint64 guid, uint8 status, if online: uint32 areaId, uint8 level, uint8 class

void FWowPacketHandler::HandleFriendList(FPacketReader& R)
{
    if (!R.CanRead(1)) return;

    uint8 Count = R.ReadU8();
    FriendsList.Empty(Count);

    UE_LOG(LogWowPacket, Log, TEXT("FRIEND_LIST: %d friends"), Count);

    for (int32 i = 0; i < Count; ++i)
    {
        if (!R.CanRead(9)) break; // minimum: guid + status

        FWowFriendInfo Friend;
        Friend.Guid = R.ReadU64();
        Friend.Status = R.ReadU8();

        // If friend is online, read additional data
        if (Friend.Status > 0) // online, AFK, DND
        {
            if (R.CanRead(6)) // areaId + level + class
            {
                Friend.AreaId = R.ReadU32();
                Friend.Level = R.ReadU8();
                Friend.Class = R.ReadU8();
            }
        }

        FriendsList.Add(Friend);

        UE_LOG(LogWowPacket, Verbose, TEXT("  Friend GUID=%llu status=%d level=%d"),
               Friend.Guid, Friend.Status, Friend.Level);
    }

    OnFriendListUpdated.Broadcast();
}

// ── SMSG_FRIEND_STATUS ──────────────────────────────────────────────────────
// uint8 resultType, uint64 guid, FString name (null-terminated)

void FWowPacketHandler::HandleFriendStatus(FPacketReader& R)
{
    if (!R.CanRead(9)) return; // result type + guid

    uint8 ResultType = R.ReadU8();
    uint64 Guid = R.ReadU64();
    FString Name = R.ReadCString();

    const TCHAR* StatusMsg = TEXT("Unknown");
    switch (ResultType)
    {
    case 0: StatusMsg = TEXT("offline"); break;
    case 1: StatusMsg = TEXT("online"); break;
    case 2: StatusMsg = TEXT("AFK"); break;
    case 3: StatusMsg = TEXT("DND"); break;
    }

    UE_LOG(LogWowPacket, Log, TEXT("FRIEND_STATUS: %s is now %s"), *Name, StatusMsg);
}

// ── SMSG_GUILD_ROSTER ───────────────────────────────────────────────────────
// uint32 memberCount, FString motd, FString guildInfo, uint32 rankCount,
// for each rank: uint32 rights + uint32 goldLimit
// for each member: uint64 guid, uint8 status, FString name, uint32 rankId, uint8 level, uint8 class, uint8 gender, uint32 zoneId, FString publicNote, FString officerNote

void FWowPacketHandler::HandleGuildRoster(FPacketReader& R)
{
    if (!R.CanRead(4)) return;

    uint32 MemberCount = R.ReadU32();
    GuildMotd = R.ReadCString();
    FString GuildInfo = R.ReadCString();

    if (!R.CanRead(4)) return;
    uint32 RankCount = R.ReadU32();

    UE_LOG(LogWowPacket, Log, TEXT("GUILD_ROSTER: %d members, %d ranks"), MemberCount, RankCount);
    UE_LOG(LogWowPacket, Verbose, TEXT("  MOTD: %s"), *GuildMotd);

    // Read rank data (rights and gold limits)
    for (uint32 i = 0; i < RankCount && R.CanRead(8); ++i)
    {
        uint32 Rights = R.ReadU32();
        uint32 GoldLimit = R.ReadU32();
        // Store rank info if needed later
    }

    GuildRoster.Empty(MemberCount);

    // Read member data
    for (uint32 i = 0; i < MemberCount; ++i)
    {
        if (!R.CanRead(9)) break; // minimum: guid + status

        FWowGuildMember Member;
        Member.Guid = R.ReadU64();
        Member.Status = R.ReadU8();
        Member.Name = R.ReadCString();

        if (!R.CanRead(11)) break; // rankId + level + class + gender + zoneId
        Member.RankId = R.ReadU32();
        Member.Level = R.ReadU8();
        Member.Class = R.ReadU8();
        uint8 Gender = R.ReadU8(); // not stored in member struct
        Member.ZoneId = R.ReadU32();

        Member.PublicNote = R.ReadCString();
        Member.OfficerNote = R.ReadCString();

        GuildRoster.Add(Member);

        UE_LOG(LogWowPacket, Verbose, TEXT("  Member: %s (level %d) rank=%d zone=%d"),
               *Member.Name, Member.Level, Member.RankId, Member.ZoneId);
    }

    OnGuildRosterUpdated.Broadcast();
}

// ── SMSG_GUILD_EVENT ────────────────────────────────────────────────────────
// uint8 eventType, uint8 stringCount, then read each string

void FWowPacketHandler::HandleGuildEvent(FPacketReader& R)
{
    if (!R.CanRead(2)) return;

    uint8 EventType = R.ReadU8();
    uint8 StringCount = R.ReadU8();

    TArray<FString> Strings;
    Strings.Reserve(StringCount);

    for (int32 i = 0; i < StringCount; ++i)
    {
        FString Str = R.ReadCString();
        Strings.Add(Str);
    }

    UE_LOG(LogWowPacket, Log, TEXT("GUILD_EVENT: type=%d strings=%d"), EventType, StringCount);
    for (int32 i = 0; i < Strings.Num(); ++i)
    {
        UE_LOG(LogWowPacket, Verbose, TEXT("  String[%d]: %s"), i, *Strings[i]);
    }
}

// ── SMSG_CHANNEL_NOTIFY ─────────────────────────────────────────────────────
// uint8 type, FString channelName

void FWowPacketHandler::HandleChannelNotify(FPacketReader& R)
{
    if (!R.CanRead(1)) return;

    uint8 Type = R.ReadU8();
    FString ChannelName = R.ReadCString();

    UE_LOG(LogWowPacket, Log, TEXT("CHANNEL_NOTIFY: type=%d channel='%s'"), Type, *ChannelName);
}

// ── SMSG_GROUP_LIST ─────────────────────────────────────────────────────────
// uint8 groupType, uint8 subgroup, uint8 flags, uint8 roles, uint64 guid (if LFG),
// uint32 counter, uint32 memberCount, for each: FString name, uint64 guid, uint8 online, uint8 subgroup, uint8 flags, uint8 roles

void FWowPacketHandler::HandleGroupList(FPacketReader& R)
{
    if (!R.CanRead(4)) return;

    uint8 GroupType = R.ReadU8();
    uint8 Subgroup = R.ReadU8();
    uint8 Flags = R.ReadU8();
    uint8 Roles = R.ReadU8();

    // Check if LFG (flags & some bit)
    if (Flags & 0x10) // example flag for LFG
    {
        if (R.CanRead(8))
        {
            uint64 LFGGuid = R.ReadU64();
        }
    }

    if (!R.CanRead(8)) return;
    uint32 Counter = R.ReadU32();
    uint32 MemberCount = R.ReadU32();

    UE_LOG(LogWowPacket, Log, TEXT("GROUP_LIST: type=%d members=%d"), GroupType, MemberCount);

    // Update group info structure
    GroupInfo.Clear();
    GroupInfo.GroupType = GroupType;
    GroupInfo.MemberCount = static_cast<uint8>(MemberCount);

    // Read member data
    for (uint32 i = 0; i < MemberCount; ++i)
    {
        FString MemberName = R.ReadCString();

        if (!R.CanRead(12)) break; // guid + online + subgroup + flags + roles
        uint64 MemberGuid = R.ReadU64();
        uint8 Online = R.ReadU8();
        uint8 MemberSubgroup = R.ReadU8();
        uint8 MemberFlags = R.ReadU8();
        uint8 MemberRoles = R.ReadU8();

        // Create group member structure
        FWowGroupMember Member;
        Member.Guid = MemberGuid;
        Member.Name = MemberName;
        Member.Group = MemberSubgroup;
        Member.Flags = MemberFlags;
        Member.Roles = MemberRoles;
        Member.Status = Online;

        // Try to get level and class from entity data
        if (auto* Entity = EntityManager.GetEntity(MemberGuid))
        {
            if (Entity->IsUnit())
            {
                Member.Level = static_cast<uint8>(Entity->GetLevel());
                Member.Class = static_cast<uint8>(Entity->GetFieldByte(UnitField::BYTES_0, 1));
            }
        }

        GroupInfo.Members.Add(Member);

        UE_LOG(LogWowPacket, Verbose, TEXT("  Member: %s (GUID=%llu online=%d level=%d class=%d)"),
               *MemberName, MemberGuid, Online, Member.Level, Member.Class);
    }

    OnGroupUpdated.Broadcast();
}

// ── SMSG_PARTY_COMMAND_RESULT ──────────────────────────────────────────────
// uint32 command, FString member, uint32 result

void FWowPacketHandler::HandlePartyCommandResult(FPacketReader& R)
{
    if (!R.CanRead(4)) return;

    uint32 Command = R.ReadU32();
    FString Member = R.ReadCString();

    if (!R.CanRead(4)) return;
    uint32 Result = R.ReadU32();

    UE_LOG(LogWowPacket, Log, TEXT("PARTY_COMMAND_RESULT: command=%d member='%s' result=%d"),
           Command, *Member, Result);

    OnPartyCommandResult.Broadcast(static_cast<uint8>(Command), Member, static_cast<uint8>(Result));
}

// ── SMSG_GROUP_INVITE ───────────────────────────────────────────────────────
// FString inviterName

void FWowPacketHandler::HandleGroupInvite(FPacketReader& R)
{
    FString InviterName = R.ReadCString();
    UE_LOG(LogWowPacket, Log, TEXT("GROUP_INVITE from '%s'"), *InviterName);

    OnGroupInviteReceived.Broadcast(InviterName);
}

// ── SMSG_SHOWTAXINODES ──────────────────────────────────────────────────────
// uint32 windowFlags, uint64 npcGuid, uint32 currentNodeId, variable-length knownNodes bitmask

void FWowPacketHandler::HandleShowTaxiNodes(FPacketReader& R)
{
    if (!R.CanRead(16)) return; // windowFlags + npcGuid + currentNodeId

    uint32 WindowFlags = R.ReadU32();
    uint64 NpcGuid = R.ReadU64();
    uint32 CurrentNodeId = R.ReadU32();

    UE_LOG(LogWowPacket, Log, TEXT("SHOWTAXINODES: npc=%llu current=%d flags=0x%X"),
           NpcGuid, CurrentNodeId, WindowFlags);

    // Clear and update taxi data
    TaxiData.Clear();
    TaxiData.NpcGuid = NpcGuid;
    TaxiData.CurrentNodeId = CurrentNodeId;
    TaxiData.bTaxiWindowOpen = true;

    // Load taxi nodes from DBC if not already loaded
    if (TaxiData.AllNodes.IsEmpty())
    {
        if (FDbcStore::Get().IsLoaded())
        {
            const auto& TaxiNodesDbc = FDbcStore::Get().TaxiNodes();
            for (const auto& DbcNode : TaxiNodesDbc.GetAll())
            {
                FWowTaxiNode Node(DbcNode.ID, DbcNode.MapID, DbcNode.X, DbcNode.Y, DbcNode.Z, DbcNode.Name);
                TaxiData.AllNodes.Add(Node);
            }
            UE_LOG(LogWowPacket, Log, TEXT("Loaded %d taxi nodes from DBC"), TaxiData.AllNodes.Num());
        }
    }

    // Read known nodes bitmask (variable length)
    // Each byte contains 8 node flags, calculate how many bytes we need
    const int32 MaxNodeId = TaxiData.AllNodes.IsEmpty() ? 256 :
        TaxiData.AllNodes.Max([](const FWowTaxiNode& A, const FWowTaxiNode& B) { return A.Id < B.Id; }).Id;
    const int32 BitmaskBytes = (MaxNodeId + 7) / 8;

    TaxiData.KnownNodes.SetNum(MaxNodeId + 1);
    TaxiData.KnownNodes.Init(false, MaxNodeId + 1);

    for (int32 ByteIndex = 0; ByteIndex < BitmaskBytes && R.CanRead(1); ++ByteIndex)
    {
        uint8 Byte = R.ReadU8();
        for (int32 BitIndex = 0; BitIndex < 8; ++BitIndex)
        {
            const int32 NodeId = ByteIndex * 8 + BitIndex;
            if (NodeId < TaxiData.KnownNodes.Num() && (Byte & (1 << BitIndex)))
            {
                TaxiData.KnownNodes[NodeId] = true;
            }
        }
    }

    int32 KnownCount = 0;
    for (bool bKnown : TaxiData.KnownNodes)
    {
        if (bKnown) KnownCount++;
    }
    UE_LOG(LogWowPacket, Log, TEXT("Player knows %d taxi nodes"), KnownCount);

    OnTaxiNodesShown.Broadcast(NpcGuid, CurrentNodeId);
}

// ── SMSG_ACTIVATETAXIREPLY ──────────────────────────────────────────────────
// uint32 result (0=OK, 1=unspecified error, etc.)

void FWowPacketHandler::HandleActivateTaxiReply(FPacketReader& R)
{
    if (!R.CanRead(4)) return;

    uint32 Result = R.ReadU32();
    UE_LOG(LogWowPacket, Log, TEXT("ACTIVATETAXIREPLY: result=%d"), Result);

    OnTaxiActivateReply.Broadcast(static_cast<uint8>(Result));

    // Close taxi window
    TaxiData.bTaxiWindowOpen = false;
}

// ── SMSG_NEW_TAXI_PATH ──────────────────────────────────────────────────────
// No payload, just refresh taxi status

void FWowPacketHandler::HandleNewTaxiPath(FPacketReader& R)
{
    UE_LOG(LogWowPacket, Log, TEXT("NEW_TAXI_PATH: refreshing taxi data"));

    // This packet indicates that the player's taxi path has been updated
    // The client should refresh its taxi information
    TaxiData.bTaxiWindowOpen = false;
}

// ── SMSG_WHO ────────────────────────────────────────────────────────────────
// uint32 displayCount, uint32 matchCount, for each: FString name, FString guildName,
// uint32 level, uint32 classId, uint32 raceId, uint32 zoneId

void FWowPacketHandler::HandleWho(FPacketReader& R)
{
    if (!R.CanRead(8)) return;

    uint32 DisplayCount = R.ReadU32();
    uint32 MatchCount = R.ReadU32();

    UE_LOG(LogWowPacket, Log, TEXT("WHO: displaying %d of %d matches"), DisplayCount, MatchCount);

    for (uint32 i = 0; i < DisplayCount; ++i)
    {
        FString Name = R.ReadCString();
        FString PlayerGuild = R.ReadCString();

        if (!R.CanRead(16)) break; // level + class + race + zone
        uint32 Level = R.ReadU32();
        uint32 ClassId = R.ReadU32();
        uint32 RaceId = R.ReadU32();
        uint32 ZoneId = R.ReadU32();

        UE_LOG(LogWowPacket, Verbose, TEXT("  %s <%s> level %d class=%d race=%d zone=%d"),
               *Name, *PlayerGuild, Level, ClassId, RaceId, ZoneId);
    }
}

// ── SMSG_QUESTGIVER_STATUS ───────────────────────────────────────────────
// uint64 guid, uint8 status

void FWowPacketHandler::HandleQuestgiverStatus(FPacketReader& R)
{
    if (!R.CanRead(9)) return;

    uint64 Guid = R.ReadU64();
    uint8 Status = R.ReadU8();

    const TCHAR* StatusMsg = TEXT("Unknown");
    switch (Status)
    {
    case 0: StatusMsg = TEXT("None"); break;
    case 1: StatusMsg = TEXT("Unavailable"); break;
    case 2: StatusMsg = TEXT("Chat"); break;
    case 3: StatusMsg = TEXT("Incomplete"); break;
    case 4: StatusMsg = TEXT("Reward Rep"); break;
    case 5: StatusMsg = TEXT("Available Rep"); break;
    case 6: StatusMsg = TEXT("Available"); break;
    case 7: StatusMsg = TEXT("Reward2"); break;
    case 8: StatusMsg = TEXT("Reward"); break;
    }

    UE_LOG(LogWowPacket, Log, TEXT("QUESTGIVER_STATUS: guid=%llu status=%d (%s)"), Guid, Status, StatusMsg);
}

// ── SMSG_QUESTGIVER_QUEST_DETAILS ────────────────────────────────────────
// uint64 guid, uint32 questId, title (null-term string), details string

void FWowPacketHandler::HandleQuestgiverQuestDetails(FPacketReader& R)
{
    if (!R.CanRead(12)) return;

    uint64 Guid = R.ReadU64();
    uint32 QuestId = R.ReadU32();

    FString Title = R.ReadCString();
    FString Details = R.ReadCString();
    FString Objectives = R.ReadCString();

    FWowQuestDetails QuestDetails;
    QuestDetails.QuestGiverGuid = Guid;
    QuestDetails.QuestId = QuestId;
    QuestDetails.Title = Title;
    QuestDetails.Details = Details;
    QuestDetails.Objectives = Objectives;

    // Parse reward items if available (simplified for now - exact structure depends on server)
    if (R.CanRead(4))
    {
        QuestDetails.RewardMoney = R.ReadU32();
    }
    if (R.CanRead(4))
    {
        QuestDetails.RewardXP = R.ReadU32();
    }

    UE_LOG(LogWowPacket, Log, TEXT("QUESTGIVER_QUEST_DETAILS: guid=%llu quest=%u title=\"%s\""),
           Guid, QuestId, *Title);

    OnQuestDialog.Broadcast(QuestDetails);
}

// ── SMSG_QUESTGIVER_OFFER_REWARD ─────────────────────────────────────────
// uint64 guid, uint32 questId, title string

void FWowPacketHandler::HandleQuestgiverOfferReward(FPacketReader& R)
{
    if (!R.CanRead(12)) return;

    uint64 Guid = R.ReadU64();
    uint32 QuestId = R.ReadU32();

    FString Title = R.ReadCString();

    FWowQuestDetails QuestDetails;
    QuestDetails.QuestGiverGuid = Guid;
    QuestDetails.QuestId = QuestId;
    QuestDetails.Title = Title;
    QuestDetails.Details = TEXT("Quest complete!");

    // Parse reward data if available
    if (R.CanRead(4))
    {
        QuestDetails.RewardMoney = R.ReadU32();
    }
    if (R.CanRead(4))
    {
        QuestDetails.RewardXP = R.ReadU32();
    }

    UE_LOG(LogWowPacket, Log, TEXT("QUESTGIVER_OFFER_REWARD: guid=%llu quest=%u title=\"%s\""),
           Guid, QuestId, *Title);

    OnQuestRewardDialog.Broadcast(QuestDetails);
}

// ── SMSG_QUEST_UPDATE_ADD_KILL ───────────────────────────────────────────
// uint32 questId, uint32 entry, uint32 count, uint32 required, uint64 guid

void FWowPacketHandler::HandleQuestUpdateAddKill(FPacketReader& R)
{
    if (!R.CanRead(24)) return;

    uint32 QuestId = R.ReadU32();
    uint32 Entry = R.ReadU32();
    uint32 Count = R.ReadU32();
    uint32 Required = R.ReadU32();
    uint64 Guid = R.ReadU64();

    // Update quest log
    for (FWowQuestLogEntry& Quest : QuestLog)
    {
        if (Quest.QuestId == QuestId)
        {
            bool bFoundObjective = false;
            for (FWowQuestObjective& Obj : Quest.Objectives)
            {
                if (Obj.CreatureOrGOId == Entry)
                {
                    Obj.Count = Count;
                    Obj.Required = Required;
                    bFoundObjective = true;
                    break;
                }
            }

            if (!bFoundObjective)
            {
                FWowQuestObjective NewObj;
                NewObj.CreatureOrGOId = Entry;
                NewObj.Count = Count;
                NewObj.Required = Required;
                Quest.Objectives.Add(NewObj);
            }
            break;
        }
    }

    UE_LOG(LogWowPacket, Log, TEXT("QUEST_UPDATE_ADD_KILL: quest=%u entry=%u count=%u/%u"),
           QuestId, Entry, Count, Required);
}

// ── SMSG_QUEST_UPDATE_COMPLETE ───────────────────────────────────────────
// uint32 questId

void FWowPacketHandler::HandleQuestUpdateComplete(FPacketReader& R)
{
    if (!R.CanRead(4)) return;

    uint32 QuestId = R.ReadU32();

    // Update quest log
    for (FWowQuestLogEntry& Quest : QuestLog)
    {
        if (Quest.QuestId == QuestId)
        {
            Quest.State = 1; // Complete
            break;
        }
    }

    UE_LOG(LogWowPacket, Log, TEXT("QUEST_UPDATE_COMPLETE: quest=%u"), QuestId);
    OnQuestComplete.Broadcast(QuestId);
}

// ── SMSG_TALENTS_INFO ────────────────────────────────────────────────────
// uint8 talentGroupCount, uint8 activeTalentGroup, then for each group:
// uint32 pointsSpent[3], uint8 talentCount, then for each talent: uint32 talentId + uint8 rank

void FWowPacketHandler::HandleTalentsInfo(FPacketReader& R)
{
    if (!R.CanRead(2)) return;

    uint8 TalentGroupCount = R.ReadU8();
    uint8 ActiveTalentGroup = R.ReadU8();

    Talents.Empty();

    for (int32 Group = 0; Group < TalentGroupCount; ++Group)
    {
        // Points spent in each tree (3 trees total)
        if (!R.CanRead(12)) return;
        uint32 PointsSpent[3];
        PointsSpent[0] = R.ReadU32();
        PointsSpent[1] = R.ReadU32();
        PointsSpent[2] = R.ReadU32();

        if (!R.CanRead(1)) return;
        uint8 TalentCount = R.ReadU8();

        for (int32 i = 0; i < TalentCount; ++i)
        {
            if (!R.CanRead(5)) break;

            uint32 TalentId = R.ReadU32();
            uint8 Rank = R.ReadU8();

            // Only store active talent group for now
            if (Group == ActiveTalentGroup)
            {
                FWowTalentInfo Talent;
                Talent.TalentId = TalentId;
                Talent.Rank = Rank;
                Talents.Add(Talent);
            }
        }
    }

    UE_LOG(LogWowPacket, Log, TEXT("TALENTS_INFO: %d groups, active=%d, %d talents"),
           TalentGroupCount, ActiveTalentGroup, Talents.Num());
    OnTalentsUpdated.Broadcast();
}

// ── SMSG_LEARNED_SPELL ───────────────────────────────────────────────────
// uint32 spellId, uint16 unknown

void FWowPacketHandler::HandleLearnedSpell(FPacketReader& R)
{
    if (!R.CanRead(6)) return;

    uint32 SpellId = R.ReadU32();
    uint16 Unknown = R.ReadU16();

    KnownSpells.Add(SpellId);

    UE_LOG(LogWowPacket, Log, TEXT("LEARNED_SPELL: spell=%u"), SpellId);
}

// ── SMSG_REMOVED_SPELL ───────────────────────────────────────────────────
// uint32 spellId

void FWowPacketHandler::HandleRemovedSpell(FPacketReader& R)
{
    if (!R.CanRead(4)) return;

    uint32 SpellId = R.ReadU32();

    KnownSpells.Remove(SpellId);

    UE_LOG(LogWowPacket, Log, TEXT("REMOVED_SPELL: spell=%u"), SpellId);
}

// ── Warden Anti-Cheat ────────────────────────────────────────────────────────

void FWowPacketHandler::InitializeWarden(const TArray<uint8>& SessionKey)
{
    WardenHandler.InitializeEncryption(SessionKey);
}

void FWowPacketHandler::HandleWardenData(FPacketReader& R)
{
    TArray<uint8> Data;
    Data.SetNumUninitialized(R.Remaining());
    if (R.Remaining() > 0)
    {
        FMemory::Memcpy(Data.GetData(), R.Data + R.Pos, R.Remaining());
    }

    WardenHandler.HandleWardenData(Data);
}

void FWowPacketHandler::SendWardenResponse(uint32 Opcode, const TArray<uint8>& Data)
{
    if (OnSendPacket.IsBound())
    {
        OnSendPacket.Execute(Opcode, Data);
    }
}

// ──────────────────────────────────────────────────────────────────────────
// Death / Corpse / Resurrection Handlers
// ──────────────────────────────────────────────────────────────────────────

// ── SMSG_CORPSE_RECLAIM_DELAY ────────────────────────────────────────────
// uint32 delayTimeInSeconds

void FWowPacketHandler::HandleCorpseReclaimDelay(FPacketReader& R)
{
    if (!R.CanRead(4)) return;

    uint32 DelayTime = R.ReadU32();
    float DelayInSeconds = static_cast<float>(DelayTime);

    UE_LOG(LogWowPacket, Log, TEXT("CORPSE_RECLAIM_DELAY: delay=%u seconds"), DelayTime);

    OnCorpseReclaimDelay.Broadcast(DelayInSeconds);
}

// ── SMSG_RESURRECT_REQUEST ───────────────────────────────────────────────
// uint64 resurrectorGuid, FString resurrectorName, uint8 unk

void FWowPacketHandler::HandleResurrectRequest(FPacketReader& R)
{
    if (!R.CanRead(8)) return;

    uint64 ResurrectorGuid = R.ReadU64();
    FString ResurrectorName = R.ReadCString();
    uint8 Unknown = R.ReadU8();

    UE_LOG(LogWowPacket, Log, TEXT("RESURRECT_REQUEST: resurrector=%llu name=%s"),
        ResurrectorGuid, *ResurrectorName);

    OnResurrectRequest.Broadcast(ResurrectorName);
}

// ── Teleport Handling ────────────────────────────────────────────────────────

void FWowPacketHandler::HandleMoveTeleport(FPacketReader& R)
{
    // Handle both old format (MSG_MOVE_TELEPORT with PackedGUID) and new format (SMSG_MOVE_TELEPORT with full GUID)

    uint64 Guid = 0;
    uint32 Flags = 0;
    uint32 Time = 0;
    uint32 MapId = 0;

    // Try to read as new format first (SMSG_MOVE_TELEPORT)
    if (R.CanRead(8 + 4 + 4 + 4*4 + 1))
    {
        Guid = R.ReadU64();
        uint32 Counter = R.ReadU32();
        MapId = R.ReadU32();
    float X = R.ReadFloat();
    float Y = R.ReadFloat();
    float Z = R.ReadFloat();
    float Orientation = R.ReadFloat();
    }
    else
    {
        // Try to read as old format (MSG_MOVE_TELEPORT with PackedGUID)
        Guid = R.ReadPackedGuid();
        Flags = R.ReadU32();
        Time = R.ReadU32();
    }

    float X = R.ReadFloat();
    float Y = R.ReadFloat();
    float Z = R.ReadFloat();
    float Orientation = R.ReadFloat();

    // Read optional unknown byte if available
    if (R.CanRead(1))
    {
        uint8 Unknown = R.ReadU8();
    }

    FVector Position(X, Y, Z);

    UE_LOG(LogWowPacket, Log, TEXT("MOVE_TELEPORT: guid=%llu map=%u pos=(%.1f,%.1f,%.1f) orient=%.2f"),
        Guid, MapId, X, Y, Z, Orientation);

    // Broadcast both legacy teleport request and new player teleport for compatibility
    OnTeleportRequest.Broadcast(Guid, Flags, Time, Position, Orientation);

    // Check if this is for the local player
    if (Guid == EntityManager.LocalPlayerGuid)
    {
        OnPlayerTeleport.Broadcast(MapId, X, Y, Z);
    }
}

void FWowPacketHandler::HandleTransferPending(FPacketReader& R)
{
    // SMSG_TRANSFER_PENDING
    // uint32 mapId, optional transport info

    uint32 MapId = R.ReadU32();

    // Note: In some versions there may be transport GUID data here,
    // but for basic implementation we'll just handle the map change

    UE_LOG(LogWowPacket, Log, TEXT("TRANSFER_PENDING: mapId=%u"), MapId);

    // This indicates a map transfer is about to happen
    // The client should expect SMSG_NEW_WORLD next
}

void FWowPacketHandler::HandleNewWorld(FPacketReader& R)
{
    // SMSG_NEW_WORLD
    // uint32 mapId + float x + float y + float z + float orientation

    if (!R.CanRead(20)) // 4 + 4*4
    {
        UE_LOG(LogWowPacket, Error, TEXT("NEW_WORLD packet too short"));
        return;
    }

    uint32 MapId = R.ReadU32();
    float X = R.ReadFloat();
    float Y = R.ReadFloat();
    float Z = R.ReadFloat();
    float Orientation = R.ReadFloat();

    UE_LOG(LogWowPacket, Log, TEXT("NEW_WORLD: map=%u pos=(%.1f,%.1f,%.1f) orient=%.2f"),
           MapId, X, Y, Z, Orientation);

    // Broadcast map transfer
    OnMapTransfer.Broadcast(MapId, X, Y, Z, Orientation);
}

// ── SMSG_NAME_QUERY_RESPONSE ─────────────────────────────────────────────────
// packedGUID, uint8 found, if found: string name, string realmName, uint8 race, uint8 gender, uint8 class

void FWowPacketHandler::HandleNameQueryResponse(FPacketReader& R)
{
    uint64 Guid = R.ReadPackedGuid();
    uint8 Found = R.ReadU8();

    if (Found)
    {
        FString Name = R.ReadCString();
        FString RealmName = R.ReadCString();
        uint8 Race = R.ReadU8();
        uint8 Gender = R.ReadU8();
        uint8 Class = R.ReadU8();

        PlayerNameCache.Add(Guid, Name);

        UE_LOG(LogWowPacket, Log, TEXT("NAME_QUERY_RESPONSE: GUID=%llu, Name=%s, Realm=%s"),
            Guid, *Name, *RealmName);

        OnPlayerNameReceived.Broadcast(Guid, Name);
    }
    else
    {
        UE_LOG(LogWowPacket, Warning, TEXT("NAME_QUERY_RESPONSE: GUID=%llu not found"), Guid);
    }
}

// ── SMSG_CREATURE_QUERY_RESPONSE ─────────────────────────────────────────────────
// uint32 entry, string name, string subName, string iconName, uint32 typeFlags,
// uint32 creatureType, uint32 creatureFamily, uint32 rank, uint32 killCredit[2],
// uint32 displayId[4], float hpMod, float manaMod, uint8 racialLeader

void FWowPacketHandler::HandleCreatureQueryResponse(FPacketReader& R)
{
    if (!R.CanRead(4)) return;

    uint32 Entry = R.ReadU32();
    FString Name = R.ReadCString();
    FString Title = R.ReadCString(); // SubName

    if (!Name.IsEmpty())
    {
        CreatureNameCache.Add(Entry, Name);
        CreatureTitleCache.Add(Entry, Title);

        UE_LOG(LogWowPacket, Log, TEXT("CREATURE_QUERY_RESPONSE: Entry=%u, Name=%s, Title=%s"),
            Entry, *Name, *Title);

        OnCreatureNameReceived.Broadcast(Entry, Name, Title);
    }

    // Skip the rest of the packet (icon name, type flags, etc.)
}
