#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FCharComponentTextureLayoutsDbcEntry
{
    uint32 ID = 0;            // col 0
    uint32 Width = 0;         // col 1
    uint32 Height = 0;        // col 2
};

class WOWDATA_API FCharComponentTextureLayoutsDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FCharComponentTextureLayoutsDbcEntry* GetById(uint32 ID) const;
    const TArray<FCharComponentTextureLayoutsDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FCharComponentTextureLayoutsDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};