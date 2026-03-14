#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FCreatureModelDataDbcEntry
{
    uint32 ID = 0;                 // 0
    uint32 Flags = 0;              // 1
    FString ModelPath;             // 2
    uint32 SizeClass = 0;          // 3
    float Scale = 1.0f;            // 4
    uint32 BloodLevelID = 0;       // 5
    uint32 FootprintID = 0;        // 6
    float FootprintLength = 0.0f;  // 7
    float FootprintWidth = 0.0f;   // 8
    float FootprintParticleScale = 0.0f; // 9
    uint32 FoleyMaterialID = 0;    // 10
    uint32 FootstepShakeSizeID = 0;// 11
    uint32 DeathThudShakeSizeID = 0;// 12
    uint32 SoundDataID = 0;        // 13
    float CollisionWidth = 0.0f;   // 14
    float CollisionHeight = 0.0f;  // 15
    float MountHeight = 0.0f;      // 16
};

class WOWDATA_API FCreatureModelDataDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FCreatureModelDataDbcEntry* GetById(uint32 ID) const;
    const TArray<FCreatureModelDataDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FCreatureModelDataDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
