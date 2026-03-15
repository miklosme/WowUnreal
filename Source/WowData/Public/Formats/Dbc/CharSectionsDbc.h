#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FCharSectionsDbcEntry
{
    uint32 ID = 0;            // col 0
    uint32 RaceID = 0;        // col 1
    uint32 SexID = 0;         // col 2
    uint32 Type = 0;          // col 3 — section type (0=Skin,1=Face,2=FacialHair,3=Hair,4=Underwear)
    FString Textures[3];      // col 4-6
    uint32 Flags = 0;         // col 7
    uint32 Variation = 0;     // col 8 — style index
    uint32 Color = 0;         // col 9 — color index
};

class WOWDATA_API FCharSectionsDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FCharSectionsDbcEntry* GetById(uint32 ID) const;
    const TArray<FCharSectionsDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FCharSectionsDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
