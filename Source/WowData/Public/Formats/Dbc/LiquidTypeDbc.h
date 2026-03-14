#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FLiquidTypeDbcEntry
{
    uint32 ID = 0;          // 0
    FString Name;           // 1
    uint32 Flags = 0;       // 2
    uint32 Type = 0;        // 3
    uint32 SoundID = 0;     // 4
    uint32 SpellID = 0;     // 5
};

class WOWDATA_API FLiquidTypeDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FLiquidTypeDbcEntry* GetById(uint32 ID) const;
    const TArray<FLiquidTypeDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FLiquidTypeDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
