#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FAnimationDataDbcEntry
{
    uint32 ID = 0;            // 0
    FString Name;             // 1
    uint32 WeaponFlags = 0;   // 2
    uint32 BodyFlags = 0;     // 3
    uint32 Flags = 0;         // 4
    uint32 Fallback = 0;      // 5
    uint32 BehaviorID = 0;    // 6
    uint32 BehaviorTier = 0;  // 7
};

class WOWDATA_API FAnimationDataDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FAnimationDataDbcEntry* GetById(uint32 ID) const;
    const TArray<FAnimationDataDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FAnimationDataDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
