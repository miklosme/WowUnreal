#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FTaxiNodesDbcEntry
{
    uint32 ID;            // 0 - Node ID
    uint32 MapID;         // 1 - Map the node is on
    float X;              // 2 - X coordinate
    float Y;              // 3 - Y coordinate
    float Z;              // 4 - Z coordinate
    FString Name;         // 5 - Node name (localized, enUS)
};

class WOWDATA_API FTaxiNodesDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FTaxiNodesDbcEntry* GetById(uint32 ID) const;
    const TArray<FTaxiNodesDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FTaxiNodesDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};