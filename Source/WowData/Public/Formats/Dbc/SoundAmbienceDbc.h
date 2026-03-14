#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FSoundAmbienceDbcEntry
{
    uint32 ID = 0;              // 0
    uint32 DayAmbience = 0;     // 1 — SoundEntries.dbc ID for daytime ambience
    uint32 NightAmbience = 0;   // 2 — SoundEntries.dbc ID for nighttime ambience
};

class WOWDATA_API FSoundAmbienceDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FSoundAmbienceDbcEntry* GetById(uint32 ID) const;
    const TArray<FSoundAmbienceDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FSoundAmbienceDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
