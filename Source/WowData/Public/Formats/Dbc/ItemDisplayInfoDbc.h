#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FItemDisplayInfoDbcEntry
{
    uint32 ID = 0;                  // 0
    FString ModelNames[2];          // 1-2
    FString ModelTextures[2];       // 3-4
    FString Icon1;                  // 5
    FString Icon2;                  // 6
    uint32 GeosetGroups[6]{};       // 7-12 (WoW 3.3.5 has 6 geoset groups)
    uint32 Flags = 0;               // 13
    uint32 SpellVisualID = 0;       // 14
    uint32 GroupSoundIndex = 0;     // 15
    uint32 HelmetGeosetVis[2]{};    // 16-17
    FString TextureOverlays[8];     // 18-25
    uint32 ItemVisual = 0;          // 26
    uint32 ParticleColorID = 0;     // 27
};

class WOWDATA_API FItemDisplayInfoDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FItemDisplayInfoDbcEntry* GetById(uint32 ID) const;
    const TArray<FItemDisplayInfoDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FItemDisplayInfoDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
