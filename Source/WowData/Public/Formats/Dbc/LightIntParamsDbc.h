#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FLightIntParamsDbcEntry
{
    static constexpr int32 MaxEntries = 16;

    uint32 ID = 0;              // 0
    uint32 EntryCount = 0;      // 1
    uint32 Times[MaxEntries]{}; // 2-17
    uint32 Values[MaxEntries]{};// 18-33
};

class WOWDATA_API FLightIntParamsDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FLightIntParamsDbcEntry* GetById(uint32 ID) const;
    const TArray<FLightIntParamsDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FLightIntParamsDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
