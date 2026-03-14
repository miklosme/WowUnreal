#include "WowPacketHandler.h"
#include "WowOpcodes.h"
#include "WowUpdateFields.h"
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
    Handlers.Add(WowOpcode::SMSG_AURA_UPDATE,              &FWowPacketHandler::HandleAuraUpdate);
    Handlers.Add(WowOpcode::SMSG_POWER_UPDATE,             &FWowPacketHandler::HandlePowerUpdate);
    Handlers.Add(WowOpcode::SMSG_MONSTER_MOVE,             &FWowPacketHandler::HandleMonsterMove);

    // Movement handlers — all use the same parser
    for (uint16 Op = WowOpcode::MSG_MOVE_START_FORWARD; Op <= WowOpcode::MSG_MOVE_SET_PITCH; ++Op)
    {
        Handlers.Add(Op, &FWowPacketHandler::HandleMovement);
    }
    Handlers.Add(WowOpcode::MSG_MOVE_ROOT, &FWowPacketHandler::HandleMovement);
    Handlers.Add(WowOpcode::MSG_MOVE_UNROOT, &FWowPacketHandler::HandleMovement);
    Handlers.Add(WowOpcode::MSG_MOVE_HEARTBEAT, &FWowPacketHandler::HandleMovement);
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

        FWowEntity& Entity = EntityManager.GetOrCreate(Guid);
        Entity.ObjectType = ObjTypeId;

        // Movement/position block
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
        ParseUpdateFields(R, Entity);

        // Extract common fields
        Entity.TypeMask = Entity.GetField(ObjectField::TYPE);
        Entity.Entry = Entity.GetField(ObjectField::ENTRY);
        Entity.Scale = Entity.GetFieldFloat(ObjectField::SCALE_X);
        if (Entity.Scale == 0.0f) Entity.Scale = 1.0f;

        EntitiesCreated++;
        EntityManager.OnEntityCreated.Broadcast(Entity);

        UE_LOG(LogWowPacket, Verbose, TEXT("Created entity GUID=%llu type=%d entry=%u pos=(%.1f,%.1f,%.1f)"),
            Guid, ObjTypeId, Entity.Entry,
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
            }
        }
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

    UE_LOG(LogWowPacket, Log, TEXT("CHAT type=%d lang=%d: %s"), Type, Language, *Message);
    OnChatMessage.Broadcast(Message);
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

    int32 NonEmpty = 0;
    for (int32 i = 0; i < ButtonCount; ++i)
    {
        uint32 PackedAction = R.ReadU32();
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

    // Skip remaining data (targets and additional cast flag data)
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
    if (Entity)
    {
        // Update entity position
        Entity->Movement.Position.X = PosX;
        Entity->Movement.Position.Y = PosY;
        Entity->Movement.Position.Z = PosZ;
    }

    uint32 PointCount = 0;

    if (MoveType == 0) // Normal movement with spline
    {
        if (!R.CanRead(12)) return;
        uint32 SplineFlags = R.ReadU32();
        uint32 Duration = R.ReadU32();
        PointCount = R.ReadU32();

        // Read waypoints
        for (uint32 i = 0; i < PointCount && R.CanRead(12); ++i)
        {
            float WPX = R.ReadFloat();
            float WPY = R.ReadFloat();
            float WPZ = R.ReadFloat();
            // Store waypoints if needed
        }
    }

    UE_LOG(LogWowPacket, Log, TEXT("MONSTER_MOVE: guid=%llu pos=(%.1f,%.1f,%.1f) points=%u"),
        TargetGuid, PosX, PosY, PosZ, PointCount);

    if (Entity)
    {
        EntityManager.OnEntityUpdated.Broadcast(*Entity);
    }
}
