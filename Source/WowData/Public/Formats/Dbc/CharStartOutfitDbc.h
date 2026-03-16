#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FCharStartOutfitDbcEntry
{
    uint32 ID = 0;            // col 0
    uint32 RaceID = 0;        // col 1 or extracted from packed col 1
    uint32 ClassID = 0;       // col 2 or extracted from packed col 1
    uint32 GenderID = 0;      // col 3 or extracted from packed col 1
    int32 ItemDisplayIds[24] = {};      // ItemDisplayId[24] (WoW 3.3.5)
    int32 ItemInventorySlots[24] = {};  // ItemInventorySlot[24] (WoW 3.3.5)
};

class WOWDATA_API FCharStartOutfitDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FCharStartOutfitDbcEntry* GetById(uint32 ID) const;
    const FCharStartOutfitDbcEntry* GetByRaceClassGender(uint32 Race, uint32 Class, uint32 Gender) const;
    const TArray<FCharStartOutfitDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FCharStartOutfitDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
    TMap<FString, int32> RaceClassGenderIndex; // "Race_Class_Gender" -> index
};