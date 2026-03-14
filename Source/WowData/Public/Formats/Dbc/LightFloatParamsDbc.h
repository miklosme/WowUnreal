#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FLightFloatParamsDbcEntry
{
    static constexpr int32 MaxEntries = 16;

    uint32 ID = 0;              // 0
    uint32 EntryCount = 0;      // 1
    uint32 Times[MaxEntries]{}; // 2-17
    float Values[MaxEntries]{}; // 18-33
};

class WOWDATA_API FLightFloatParamsDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FLightFloatParamsDbcEntry* GetById(uint32 ID) const;
    const TArray<FLightFloatParamsDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FLightFloatParamsDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
