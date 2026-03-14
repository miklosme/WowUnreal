#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FAreaTableDbcEntry
{
    uint32 ID;              // 0
    uint32 MapID;           // 1
    uint32 ParentAreaID;    // 2 (zone ID, 0 if this IS a zone)
    uint32 ExploreFlag;     // 3
    uint32 Flags;           // 4
    uint32 AmbienceID = 0;  // 7  — SoundAmbience.dbc ID
    uint32 ZoneMusicID = 0; // 8  — ZoneMusic.dbc ID
    uint32 IntroMusicID = 0;// 9  — ZoneIntroMusicTable.dbc ID
    int32  AreaLevel;       // 10
    FString Name;           // 11 (localized, enUS)
    uint32 Team;            // 28 (0=neutral, 67=Horde, 469=Alliance)
};

class WOWDATA_API FAreaTableDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FAreaTableDbcEntry* GetById(uint32 ID) const;
    const TArray<FAreaTableDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FAreaTableDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
