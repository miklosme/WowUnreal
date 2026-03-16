#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FCharComponentTextureSectionsDbcEntry
{
    uint32 ID = 0;                            // col 0
    uint32 CharComponentTextureLayoutID = 0; // col 1
    uint32 SectionType = 0;                  // col 2
    uint32 X = 0;                            // col 3
    uint32 Y = 0;                            // col 4
    uint32 Width = 0;                        // col 5
    uint32 Height = 0;                       // col 6
};

class WOWDATA_API FCharComponentTextureSectionsDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FCharComponentTextureSectionsDbcEntry* GetById(uint32 ID) const;
    const TArray<FCharComponentTextureSectionsDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

    /** Get all sections for a specific layout ID */
    TArray<const FCharComponentTextureSectionsDbcEntry*> GetByLayoutId(uint32 LayoutID) const;

private:
    TArray<FCharComponentTextureSectionsDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
    TMultiMap<uint32, int32> LayoutIdIndex;
};