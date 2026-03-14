#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FCreatureDisplayInfoDbcEntry
{
    uint32 ID = 0;                    // 0
    uint32 ModelID = 0;               // 1
    uint32 SoundID = 0;               // 2
    uint32 ExtendedDisplayInfoID = 0; // 3
    float Scale = 1.0f;               // 4
    uint32 Opacity = 0;               // 5
    FString Texture1;                 // 6
    FString Texture2;                 // 7
    FString Texture3;                 // 8
    FString PortraitTextureName;      // 9
    uint32 BloodLevel = 0;            // 10
    uint32 BloodID = 0;               // 11
    uint32 NPCSoundID = 0;            // 12
    uint32 ParticleColorID = 0;       // 13
    uint32 CreatureGeosetData = 0;    // 14
    uint32 ObjectEffectPackageID = 0; // 15
};

class WOWDATA_API FCreatureDisplayInfoDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FCreatureDisplayInfoDbcEntry* GetById(uint32 ID) const;
    const TArray<FCreatureDisplayInfoDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FCreatureDisplayInfoDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
