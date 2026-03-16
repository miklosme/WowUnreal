#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FCharHairGeosetsDbcEntry
{
    uint32 ID = 0;            // col 0
    uint32 RaceID = 0;        // col 1
    uint32 SexID = 0;         // col 2
    uint32 VariationID = 0;   // col 3
    uint32 GeosetID = 0;      // col 4
    uint32 Showscalp = 0;     // col 5
};

class WOWDATA_API FCharHairGeosetsDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FCharHairGeosetsDbcEntry* GetById(uint32 ID) const;
    const FCharHairGeosetsDbcEntry* GetByRaceGenderVariation(uint32 RaceId, uint32 Gender, uint32 Variation) const;
    const TArray<FCharHairGeosetsDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FCharHairGeosetsDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
    TMap<uint64, int32> RaceGenderVariationIndex; // Combined key: (RaceId << 32) | (Gender << 16) | Variation
};