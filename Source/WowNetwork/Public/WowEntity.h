#pragma once
#include "CoreMinimal.h"
#include "WowUpdateFields.h"

// Spline movement waypoint for SMSG_MONSTER_MOVE
struct FWowSplineWaypoint
{
    FVector Position = FVector::ZeroVector;

    FWowSplineWaypoint() = default;
    FWowSplineWaypoint(const FVector& InPosition) : Position(InPosition) {}
};

// Movement info for a living entity
struct FWowMovementInfo
{
    FVector Position = FVector::ZeroVector;
    float Orientation = 0.0f;
    uint32 MoveFlags = 0;
    uint16 MoveFlags2 = 0;
    uint32 FallTime = 0;
    float RunSpeed = 7.0f;
    float RunBackSpeed = 4.5f;
    float SwimSpeed = 4.722f;
    float SwimBackSpeed = 2.5f;
    float FlightSpeed = 7.0f;
    float FlightBackSpeed = 4.5f;
    float TurnRate = 3.1415f;
    float PitchRate = 0.0f;
    float WalkSpeed = 2.5f;
    uint64 TransportGuid = 0;
    FVector TransportOffset = FVector::ZeroVector;
    float TransportOrientation = 0.0f;

    // Spline movement data from SMSG_MONSTER_MOVE
    bool bHasActiveSpline = false;
    uint32 SplineId = 0;
    uint32 SplineFlags = 0;
    uint32 SplineDuration = 0;  // Movement duration in milliseconds
    float SplineElapsed = 0.0f; // Time elapsed since spline started
    TArray<FWowSplineWaypoint> SplineWaypoints;
    FVector SplineStartPosition = FVector::ZeroVector;
    uint8 SplineMoveType = 0; // 0=normal, 1=stop, 2=facing point, 3=facing angle, 4=facing target
};

// Aura information for a single aura slot
struct FAuraInfo
{
    uint32 SpellId = 0;
    uint8 Flags = 0;
    uint8 Level = 0;
    uint8 Charges = 0;
    uint64 CasterGuid = 0;
    bool bActive = false;
};

// WoW item information
struct FWowItem
{
    uint64 Guid = 0;
    uint32 Entry = 0;   // Item template ID
    uint32 Count = 0;
    uint8 Bag = 0;       // 255 = backpack
    uint8 Slot = 0;
    uint8 Quality = 0;   // 0=grey, 1=white, 2=green, 3=blue, 4=purple, 5=orange
};

// Loot item information
struct FWowLootItem
{
    uint32 ItemId = 0;
    uint32 Count = 0;
    uint32 DisplayId = 0;
    uint32 RandomSuffix = 0;
    uint32 RandomProperty = 0;
    uint8 Quality = 0;
    uint8 Index = 0;
    uint8 SlotType = 0;
    bool bLooted = false;
};

// Vendor item information
struct FWowVendorItem
{
    uint32 Slot = 0;
    uint32 ItemId = 0;
    uint32 DisplayId = 0;
    uint32 MaxCount = 0;
    uint32 Price = 0;
    uint32 MaxDurability = 0;
    uint32 BuyCount = 0;
    uint32 ExtendedCost = 0;
};

// Quest reward item
struct FWowQuestRewardItem
{
    uint32 ItemId = 0;
    uint32 Count = 0;
    uint32 DisplayId = 0;
};

// Quest details for dialog
struct FWowQuestDetails
{
    uint64 QuestGiverGuid = 0;
    uint32 QuestId = 0;
    FString Title;
    FString Details;
    FString Objectives;
    TArray<FWowQuestRewardItem> RewardItems;
    TArray<FWowQuestRewardItem> ChoiceRewards;
    uint32 RewardMoney = 0;
    uint32 RewardXP = 0;
};

// WoW 3.3.5a NPC flags — determine what interactions are available
namespace WowNpcFlags
{
    inline constexpr uint32 NONE            = 0x00000000;
    inline constexpr uint32 GOSSIP          = 0x00000001;
    inline constexpr uint32 QUESTGIVER      = 0x00000002;
    inline constexpr uint32 VENDOR          = 0x00000080; // Generic vendor (sells items)
    inline constexpr uint32 VENDOR_AMMO     = 0x00000100;
    inline constexpr uint32 VENDOR_FOOD     = 0x00000200;
    inline constexpr uint32 VENDOR_POISON   = 0x00000400;
    inline constexpr uint32 VENDOR_REAGENT  = 0x00000800;
    inline constexpr uint32 REPAIR          = 0x00001000;
    inline constexpr uint32 FLIGHTMASTER    = 0x00002000;
    inline constexpr uint32 TRAINER         = 0x00000010;
    inline constexpr uint32 TRAINER_CLASS   = 0x00000020;
    inline constexpr uint32 TRAINER_PROF    = 0x00000040;
    inline constexpr uint32 INNKEEPER       = 0x00010000;
    inline constexpr uint32 BANKER          = 0x00020000;
    inline constexpr uint32 PETITIONER      = 0x00040000;
    inline constexpr uint32 AUCTIONEER      = 0x00200000;
    inline constexpr uint32 STABLEMASTER    = 0x00400000;
    inline constexpr uint32 MAILBOX         = 0x04000000;

    // Combined mask: any flag that means "talk to me" instead of "attack me"
    inline constexpr uint32 INTERACTABLE = GOSSIP | QUESTGIVER | VENDOR | REPAIR
        | FLIGHTMASTER | TRAINER | TRAINER_CLASS | TRAINER_PROF
        | INNKEEPER | BANKER | PETITIONER | AUCTIONEER | STABLEMASTER;
}

// WoW 3.3.5a unit stand states (UNIT_FIELD_BYTES_1 byte 0)
namespace WowStandState
{
    inline constexpr uint8 STAND     = 0;
    inline constexpr uint8 SIT       = 1;
    inline constexpr uint8 SIT_CHAIR = 2;
    inline constexpr uint8 SLEEP     = 3;
    inline constexpr uint8 SIT_LOW   = 4;
    inline constexpr uint8 SIT_MED   = 5;
    inline constexpr uint8 SIT_HIGH  = 6;
    inline constexpr uint8 DEAD      = 7;
    inline constexpr uint8 KNEEL     = 8;
    inline constexpr uint8 SUBMERGED = 9;
}

// WoW 3.3.5a unit dynamic flags
namespace WowDynFlags
{
    inline constexpr uint32 NONE         = 0x0000;
    inline constexpr uint32 LOOTABLE     = 0x0001;
    inline constexpr uint32 TRACK_UNIT   = 0x0002;
    inline constexpr uint32 TAPPED       = 0x0004;
    inline constexpr uint32 TAPPED_BY_PLAYER = 0x0008;
    inline constexpr uint32 DEAD         = 0x0020;
}

enum class EWowEntityKind : uint8
{
    Object,
    Item,
    Container,
    Unit,
    Player,
    GameObject,
    DynamicObject,
    Corpse
};

// Base entity — represents any WoW object tracked by the client
struct WOWNETWORK_API FWowEntity
{
    uint64 Guid = 0;
    uint8 ObjectTypeId = 0; // Server object type id from CREATE_OBJECT
    uint32 TypeMask = 0;  // WowTypeMask bitmask
    uint32 Entry = 0;
    float Scale = 1.0f;

    FWowMovementInfo Movement;

    // Aura slots (max 64 aura slots in 3.3.5a)
    TArray<FAuraInfo> Auras;

    // Raw update field values (indexed by field offset)
    TMap<uint16, uint32> Fields;

    FWowEntity()
    {
        // Initialize 64 aura slots
        Auras.SetNum(64);
    }

    FWowEntity(const FWowEntity&) = default;
    FWowEntity(FWowEntity&&) = default;
    FWowEntity& operator=(const FWowEntity&) = default;
    FWowEntity& operator=(FWowEntity&&) = default;
    virtual ~FWowEntity() = default;

    virtual EWowEntityKind GetEntityKind() const { return EWowEntityKind::Object; }
    virtual const TCHAR* GetEntityKindName() const { return TEXT("Object"); }

    // Convenience accessors
    bool IsPlayer() const { return GetEntityKind() == EWowEntityKind::Player || (TypeMask & WowTypeMask::PLAYER) != 0; }
    bool IsUnit() const
    {
        const EWowEntityKind Kind = GetEntityKind();
        return Kind == EWowEntityKind::Unit || Kind == EWowEntityKind::Player
            || (TypeMask & (WowTypeMask::UNIT | WowTypeMask::PLAYER)) != 0;
    }
    bool IsGameObject() const { return GetEntityKind() == EWowEntityKind::GameObject || (TypeMask & WowTypeMask::GAMEOBJECT) != 0; }
    bool IsItem() const
    {
        const EWowEntityKind Kind = GetEntityKind();
        return Kind == EWowEntityKind::Item || Kind == EWowEntityKind::Container
            || (TypeMask & (WowTypeMask::ITEM | WowTypeMask::CONTAINER)) != 0;
    }
    bool IsContainer() const { return GetEntityKind() == EWowEntityKind::Container || (TypeMask & WowTypeMask::CONTAINER) != 0; }
    bool IsDynamicObject() const { return GetEntityKind() == EWowEntityKind::DynamicObject || (TypeMask & WowTypeMask::DYNAMICOBJECT) != 0; }
    bool IsCorpse() const { return GetEntityKind() == EWowEntityKind::Corpse || (TypeMask & WowTypeMask::CORPSE) != 0; }

    uint32 GetField(uint16 Index) const
    {
        const uint32* Val = Fields.Find(Index);
        return Val ? *Val : 0;
    }

    uint64 GetField64(uint16 Index) const
    {
        return static_cast<uint64>(GetField(Index)) | (static_cast<uint64>(GetField(Index + 1)) << 32);
    }

    float GetFieldFloat(uint16 Index) const
    {
        uint32 Raw = GetField(Index);
        float Value = 0.0f;
        FMemory::Memcpy(&Value, &Raw, sizeof(float));
        return Value;
    }

    uint8 GetFieldByte(uint16 Index, uint8 ByteOffset) const
    {
        return static_cast<uint8>((GetField(Index) >> (ByteOffset * 8)) & 0xFF);
    }

    void SetField(uint16 Index, uint32 Value)
    {
        Fields.Add(Index, Value);
    }

    // Common field shortcuts
    int32 GetHealth() const { return static_cast<int32>(GetField(UnitField::HEALTH)); }
    int32 GetMaxHealth() const { return static_cast<int32>(GetField(UnitField::MAXHEALTH)); }
    int32 GetLevel() const { return static_cast<int32>(GetField(UnitField::LEVEL)); }
    uint32 GetDisplayId() const { return GetField(UnitField::DISPLAYID); }
    uint32 GetNativeDisplayId() const { return GetField(UnitField::NATIVEDISPLAYID); }
};

struct WOWNETWORK_API FWowItemEntity : public FWowEntity
{
    FWowItemEntity() = default;
    explicit FWowItemEntity(const FWowEntity& Other) : FWowEntity(Other) {}

    EWowEntityKind GetEntityKind() const override { return EWowEntityKind::Item; }
    const TCHAR* GetEntityKindName() const override { return TEXT("Item"); }

    uint64 GetOwnerGuid() const { return GetField64(ItemField::OWNER); }
    uint64 GetContainedGuid() const { return GetField64(ItemField::CONTAINED); }
    uint64 GetCreatorGuid() const { return GetField64(ItemField::CREATOR); }
    int32 GetStackCount() const { return static_cast<int32>(GetField(ItemField::STACK_COUNT)); }
    int32 GetDuration() const { return static_cast<int32>(GetField(ItemField::DURATION)); }
    uint32 GetItemFlags() const { return GetField(ItemField::FLAGS); }
    int32 GetDurability() const { return static_cast<int32>(GetField(ItemField::DURABILITY)); }
    int32 GetMaxDurability() const { return static_cast<int32>(GetField(ItemField::MAX_DURABILITY)); }

    uint32 GetSpellCharges(int32 Slot) const
    {
        return (Slot >= 0 && Slot < 5) ? GetField(ItemField::SPELL_CHARGES + Slot) : 0;
    }

    uint32 GetEnchantmentId(int32 Slot) const
    {
        return (Slot >= 0 && Slot < 12) ? GetField(ItemField::ENCHANTMENT_1_1 + Slot * 3) : 0;
    }

    uint32 GetEnchantmentDuration(int32 Slot) const
    {
        return (Slot >= 0 && Slot < 12) ? GetField(ItemField::ENCHANTMENT_1_1 + Slot * 3 + 1) : 0;
    }

    uint32 GetEnchantmentCharges(int32 Slot) const
    {
        return (Slot >= 0 && Slot < 12) ? GetField(ItemField::ENCHANTMENT_1_1 + Slot * 3 + 2) : 0;
    }
};

struct WOWNETWORK_API FWowContainerEntity : public FWowItemEntity
{
    FWowContainerEntity() = default;
    explicit FWowContainerEntity(const FWowEntity& Other) : FWowItemEntity(Other) {}

    EWowEntityKind GetEntityKind() const override { return EWowEntityKind::Container; }
    const TCHAR* GetEntityKindName() const override { return TEXT("Container"); }

    int32 GetNumSlots() const { return static_cast<int32>(GetField(ContainerField::NUM_SLOTS)); }

    uint64 GetItemGuidAtSlot(int32 SlotIndex) const
    {
        return (SlotIndex >= 0 && SlotIndex < 36) ? GetField64(ContainerField::SLOT_1 + SlotIndex * 2) : 0;
    }
};

struct WOWNETWORK_API FWowUnitEntity : public FWowEntity
{
    FWowUnitEntity() = default;
    explicit FWowUnitEntity(const FWowEntity& Other) : FWowEntity(Other) {}

    EWowEntityKind GetEntityKind() const override { return EWowEntityKind::Unit; }
    const TCHAR* GetEntityKindName() const override { return TEXT("Unit"); }

    int32 GetPower(uint8 PowerType) const
    {
        return (PowerType < 7) ? static_cast<int32>(GetField(UnitField::POWER1 + PowerType)) : 0;
    }

    int32 GetMaxPower(uint8 PowerType) const
    {
        return (PowerType < 7) ? static_cast<int32>(GetField(UnitField::MAXPOWER1 + PowerType)) : 0;
    }

    uint32 GetFactionTemplate() const { return GetField(UnitField::FACTIONTEMPLATE); }
    uint32 GetUnitFlags() const { return GetField(UnitField::FLAGS); }
    uint32 GetUnitFlags2() const { return GetField(UnitField::FLAGS_2); }
    uint32 GetNpcFlags() const { return GetField(UnitField::NPC_FLAGS); }
    uint32 GetMountDisplayId() const { return GetField(UnitField::MOUNTDISPLAYID); }

    /** Check if this NPC has any interactable flags (gossip, questgiver, vendor, etc.) */
    bool IsInteractable() const { return (GetNpcFlags() & WowNpcFlags::INTERACTABLE) != 0; }
    bool IsQuestGiver() const { return (GetNpcFlags() & WowNpcFlags::QUESTGIVER) != 0; }
    bool IsVendor() const { return (GetNpcFlags() & WowNpcFlags::VENDOR) != 0; }
    bool IsFlightMaster() const { return (GetNpcFlags() & WowNpcFlags::FLIGHTMASTER) != 0; }

    // Stand state (from BYTES_1, byte 0) — controls idle pose: stand, sit, sleep, kneel, dead
    uint8 GetStandState() const { return GetFieldByte(UnitField::BYTES_1, 0); }
    bool IsDead() const { return GetStandState() == WowStandState::DEAD || GetHealth() <= 0; }
    bool IsSitting() const { return GetStandState() >= WowStandState::SIT && GetStandState() <= WowStandState::SIT_HIGH; }
    bool IsSleeping() const { return GetStandState() == WowStandState::SLEEP; }
    bool IsKneeling() const { return GetStandState() == WowStandState::KNEEL; }

    // Dynamic flags — lootable, tapped, etc.
    uint32 GetDynamicFlags() const { return GetField(UnitField::DYNAMIC_FLAGS); }
    bool IsLootable() const { return (GetDynamicFlags() & WowDynFlags::LOOTABLE) != 0; }
    bool IsTapped() const { return (GetDynamicFlags() & WowDynFlags::TAPPED) != 0; }

    // NPC emote state (looping emote animation)
    uint32 GetEmoteState() const { return GetField(UnitField::NPC_EMOTESTATE); }

    // Stats (Strength, Agility, Stamina, Intellect, Spirit)
    int32 GetStrength() const { return static_cast<int32>(GetField(UnitField::STAT0)); }
    int32 GetAgility() const { return static_cast<int32>(GetField(UnitField::STAT1)); }
    int32 GetStamina() const { return static_cast<int32>(GetField(UnitField::STAT2)); }
    int32 GetIntellect() const { return static_cast<int32>(GetField(UnitField::STAT3)); }
    int32 GetSpirit() const { return static_cast<int32>(GetField(UnitField::STAT4)); }

    // Resistances (Physical=armor at index 0, then Holy, Fire, Nature, Frost, Shadow, Arcane)
    int32 GetArmor() const { return static_cast<int32>(GetField(UnitField::RESISTANCES)); }
    int32 GetResistance(int32 School) const
    {
        return (School >= 0 && School < 7) ? static_cast<int32>(GetField(UnitField::RESISTANCES + School)) : 0;
    }

    // Attack Power
    int32 GetAttackPower() const { return static_cast<int32>(GetField(UnitField::ATTACK_POWER)); }
    int32 GetRangedAttackPower() const { return static_cast<int32>(GetField(UnitField::RANGED_ATTACK_POWER)); }

    // Sheath state (from BYTES_2, byte 0) — weapon visibility
    uint8 GetSheathState() const { return GetFieldByte(UnitField::BYTES_2, 0); }
    uint8 GetRaceId() const { return GetFieldByte(UnitField::BYTES_0, 0); }
    uint8 GetClassId() const { return GetFieldByte(UnitField::BYTES_0, 1); }
    uint8 GetGenderId() const { return GetFieldByte(UnitField::BYTES_0, 2); }
    uint8 GetPowerTypeId() const { return GetFieldByte(UnitField::BYTES_0, 3); }
};

struct WOWNETWORK_API FWowPlayerEntity : public FWowUnitEntity
{
    FWowPlayerEntity() = default;
    explicit FWowPlayerEntity(const FWowEntity& Other) : FWowUnitEntity(Other) {}

    EWowEntityKind GetEntityKind() const override { return EWowEntityKind::Player; }
    const TCHAR* GetEntityKindName() const override { return TEXT("Player"); }

    uint32 GetPlayerFlags() const { return GetField(PlayerField::FLAGS); }
    uint32 GetPlayerBytes() const { return GetField(PlayerField::BYTES); }
    uint32 GetPlayerBytes2() const { return GetField(PlayerField::BYTES_2); }
    uint32 GetXp() const { return GetField(PlayerField::XP); }
    uint32 GetNextLevelXp() const { return GetField(PlayerField::NEXT_LEVEL_XP); }
    uint32 GetCoinage() const { return GetField(PlayerField::COINAGE); }
    uint8 GetBankBagSlotCount() const { return GetFieldByte(PlayerField::BYTES_2, 2); }

    // Equipment and inventory accessors
    uint64 GetEquipmentItemGuid(uint8 SlotIndex) const
    {
        return (SlotIndex < 19) ? GetField64(PlayerField::INV_SLOT_HEAD + SlotIndex * 2) : 0;
    }

    uint64 GetBackpackItemGuid(uint8 SlotIndex) const
    {
        return (SlotIndex < 16) ? GetField64(PlayerField::PACK_SLOT_START + SlotIndex * 2) : 0;
    }

    uint64 GetBagGuid(uint8 BagIndex) const
    {
        if (BagIndex >= 1 && BagIndex <= 4)
            return GetField64(PlayerField::INV_SLOT_BAG_1 + (BagIndex - 1) * 2);
        return 0;
    }

    uint64 GetBankItemGuid(uint8 SlotIndex) const
    {
        return (SlotIndex < 28) ? GetField64(PlayerField::BANK_SLOT_START + SlotIndex * 2) : 0;
    }

    uint64 GetBankBagGuid(uint8 BagIndex) const
    {
        if (BagIndex < 7)
            return GetField64(PlayerField::BANKBAG_SLOT_1 + BagIndex * 2);
        return 0;
    }

    // Combat ratings (for players only)
    int32 GetCombatRating(int32 RatingType) const
    {
        return (RatingType >= 0 && RatingType < 25) ? static_cast<int32>(GetField(PlayerField::COMBAT_RATING_1 + RatingType)) : 0;
    }
};

struct WOWNETWORK_API FWowGameObjectEntity : public FWowEntity
{
    FWowGameObjectEntity() = default;
    explicit FWowGameObjectEntity(const FWowEntity& Other) : FWowEntity(Other) {}

    EWowEntityKind GetEntityKind() const override { return EWowEntityKind::GameObject; }
    const TCHAR* GetEntityKindName() const override { return TEXT("GameObject"); }

    uint64 GetCreatorGuid() const { return GetField64(GameObjectField::CREATED_BY); }
    uint32 GetGameObjectDisplayId() const { return GetField(GameObjectField::DISPLAY_ID); }
    uint32 GetGameObjectFlags() const { return GetField(GameObjectField::FLAGS); }
    uint32 GetFaction() const { return GetField(GameObjectField::FACTION); }
    int32 GetGameObjectLevel() const { return static_cast<int32>(GetField(GameObjectField::LEVEL)); }
    float GetParentRotationComponent(int32 ComponentIndex) const
    {
        return (ComponentIndex >= 0 && ComponentIndex < 4) ? GetFieldFloat(GameObjectField::PARENT_ROTATION + ComponentIndex) : 0.0f;
    }
};

struct WOWNETWORK_API FWowDynamicObjectEntity : public FWowEntity
{
    FWowDynamicObjectEntity() = default;
    explicit FWowDynamicObjectEntity(const FWowEntity& Other) : FWowEntity(Other) {}

    EWowEntityKind GetEntityKind() const override { return EWowEntityKind::DynamicObject; }
    const TCHAR* GetEntityKindName() const override { return TEXT("DynamicObject"); }

    uint64 GetCasterGuid() const { return GetField64(DynamicObjectField::CASTER); }
    uint32 GetSpellId() const { return GetField(DynamicObjectField::SPELL_ID); }
    float GetRadius() const { return GetFieldFloat(DynamicObjectField::RADIUS); }
    uint32 GetCastTime() const { return GetField(DynamicObjectField::CAST_TIME); }
};

struct WOWNETWORK_API FWowCorpseEntity : public FWowEntity
{
    FWowCorpseEntity() = default;
    explicit FWowCorpseEntity(const FWowEntity& Other) : FWowEntity(Other) {}

    EWowEntityKind GetEntityKind() const override { return EWowEntityKind::Corpse; }
    const TCHAR* GetEntityKindName() const override { return TEXT("Corpse"); }

    uint64 GetOwnerGuid() const { return GetField64(CorpseField::OWNER); }
};

// Quest objective tracking
struct FWowQuestObjective
{
    uint32 CreatureOrGOId = 0;
    uint32 Count = 0;
    uint32 Required = 0;
};

// Quest log entry
struct FWowQuestLogEntry
{
    uint32 QuestId = 0;
    uint32 State = 0; // 0=not complete, 1=complete, 2=failed
    TArray<FWowQuestObjective> Objectives;
};

// Talent information
struct FWowTalentInfo
{
    uint32 TalentId = 0;
    uint8 Rank = 0; // 0-5
};

// Friend information for the social system
struct FWowFriendInfo
{
    uint64 Guid = 0;
    FString Name;
    uint8 Status = 0;    // 0=offline, 1=online, 2=AFK, 3=DND
    uint32 AreaId = 0;
    uint8 Level = 0;
    uint8 Class = 0;
    FString Note;
};

// Guild member information
struct FWowGuildMember
{
    uint64 Guid = 0;
    FString Name;
    uint8 Status = 0;
    uint8 Level = 0;
    uint8 Class = 0;
    uint32 ZoneId = 0;
    uint8 RankId = 0;
    FString RankName;
    FString PublicNote;
    FString OfficerNote;
};

// Group/Party member information
struct FWowGroupMember
{
    uint64 Guid = 0;
    FString Name;
    uint8 Group = 0;        // Subgroup index (0-7)
    uint8 Flags = 0;        // Member flags
    uint8 Roles = 0;        // Tank/Healer/DPS roles
    uint8 Status = 0;       // Online status
    uint8 Level = 0;        // Member level
    uint8 Class = 0;        // Member class
};

// Group/Party information
struct FWowGroupInfo
{
    uint8 MemberCount = 0;
    uint8 GroupType = 0;    // 0=party, 1=raid
    uint64 LeaderGuid = 0;
    TArray<FWowGroupMember> Members;

    void Clear()
    {
        MemberCount = 0;
        GroupType = 0;
        LeaderGuid = 0;
        Members.Empty();
    }

    bool IsEmpty() const
    {
        return MemberCount == 0 || Members.IsEmpty();
    }

    const FWowGroupMember* FindMember(uint64 Guid) const
    {
        return Members.FindByPredicate([Guid](const FWowGroupMember& Member)
        {
            return Member.Guid == Guid;
        });
    }
};

// Taxi node information
struct FWowTaxiNode
{
    uint32 Id = 0;
    uint32 MapId = 0;
    float X = 0.0f;
    float Y = 0.0f;
    float Z = 0.0f;
    FString Name;

    FWowTaxiNode() = default;
    FWowTaxiNode(uint32 InId, uint32 InMapId, float InX, float InY, float InZ, const FString& InName)
        : Id(InId), MapId(InMapId), X(InX), Y(InY), Z(InZ), Name(InName) {}
};

// Taxi system information
struct FWowTaxiData
{
    uint64 NpcGuid = 0;
    uint32 CurrentNodeId = 0;
    TArray<bool> KnownNodes;        // Known taxi nodes (bitmask converted to array)
    TArray<FWowTaxiNode> AllNodes;  // All taxi nodes from DBC
    bool bTaxiWindowOpen = false;

    void Clear()
    {
        NpcGuid = 0;
        CurrentNodeId = 0;
        KnownNodes.Empty();
        bTaxiWindowOpen = false;
    }

    bool IsNodeKnown(uint32 NodeId) const
    {
        return NodeId < static_cast<uint32>(KnownNodes.Num()) && KnownNodes[NodeId];
    }

    const FWowTaxiNode* FindNode(uint32 NodeId) const
    {
        return AllNodes.FindByPredicate([NodeId](const FWowTaxiNode& Node)
        {
            return Node.Id == NodeId;
        });
    }
};
