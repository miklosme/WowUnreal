#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FZoneMusicDbcEntry
{
    uint32 ID = 0;           // 0
    FString Name;            // 1
    uint32 SoundDayID = 0;   // 2  — SoundEntries.dbc ID for day music
    uint32 SoundNightID = 0; // 3  — SoundEntries.dbc ID for night music
};

class WOWDATA_API FZoneMusicDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FZoneMusicDbcEntry* GetById(uint32 ID) const;
    const TArray<FZoneMusicDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FZoneMusicDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
