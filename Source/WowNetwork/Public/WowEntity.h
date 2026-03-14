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
