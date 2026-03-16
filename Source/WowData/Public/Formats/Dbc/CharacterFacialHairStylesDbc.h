#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FCharacterFacialHairStylesDbcEntry
{
    uint32 RaceID = 0;        // col 0 (no ID column)
    uint32 SexID = 0;         // col 1
    uint32 VariationID = 0;   // col 2
    uint32 Geosets[5] = {0};  // col 3-7 (Geoset0-Geoset4)
};

class WOWDATA_API FCharacterFacialHairStylesDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FCharacterFacialHairStylesDbcEntry* GetByRaceGenderVariation(uint32 RaceId, uint32 Gender, uint32 Variation) const;
    const TArray<FCharacterFacialHairStylesDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FCharacterFacialHairStylesDbcEntry> Entries;
    TMap<uint64, int32> RaceGenderVariationIndex; // Combined key: (RaceId << 32) | (Gender << 16) | Variation
};