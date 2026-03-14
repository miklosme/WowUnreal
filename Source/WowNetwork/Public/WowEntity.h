#pragma once
#include "CoreMinimal.h"
#include "WowUpdateFields.h"

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
    uint8 Quality = 0;
    uint8 Index = 0;
    bool bLooted = false;
};

// Base entity — represents any WoW object tracked by the client
struct WOWNETWORK_API FWowEntity
{
    uint64 Guid = 0;
    uint8 ObjectType = 0; // UpdateType that created it
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

    // Convenience accessors
    bool IsPlayer() const { return (TypeMask & WowTypeMask::PLAYER) != 0; }
    bool IsUnit() const { return (TypeMask & WowTypeMask::UNIT) != 0; }
    bool IsGameObject() const { return (TypeMask & WowTypeMask::GAMEOBJECT) != 0; }
    bool IsItem() const { return (TypeMask & WowTypeMask::ITEM) != 0; }

    uint32 GetField(uint16 Index) const
    {
        const uint32* Val = Fields.Find(Index);
        return Val ? *Val : 0;
    }

    float GetFieldFloat(uint16 Index) const
    {
        uint32 Raw = GetField(Index);
        return *reinterpret_cast<const float*>(&Raw);
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
