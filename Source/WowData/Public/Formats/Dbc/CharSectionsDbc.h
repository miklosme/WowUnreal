#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FCharSectionsDbcEntry
{
    uint32 ID = 0;            // 0
    uint32 RaceID = 0;        // 1
    uint32 SexID = 0;         // 2
    uint32 Variation = 0;     // 3
    FString Textures[3];      // 4-6
    uint32 Flags = 0;         // 7
    uint32 Type = 0;          // 8
    uint32 Color = 0;         // 9
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
