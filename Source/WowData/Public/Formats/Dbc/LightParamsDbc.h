#pragma once
#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FLightParamsDbcEntry
{
    uint32 ID;                  // 0
    uint32 HighlightSky;        // 1
    uint32 SkyboxID;            // 2 (ref LightSkybox.dbc)
    uint32 CloudTypeID;         // 3
    float  Glow;                // 4
    float  WaterShallowAlpha;   // 5
    float  WaterDeepAlpha;      // 6
    float  OceanShallowAlpha;   // 7
    float  OceanDeepAlpha;      // 8
};

class WOWDATA_API FLightParamsDbc
{
public:
    bool Load(const FDbcParser& Parser);
    const FLightParamsDbcEntry* GetById(uint32 ID) const;
    const TArray<FLightParamsDbcEntry>& GetAll() const { return Entries; }
    int32 Num() const { return Entries.Num(); }

private:
    TArray<FLightParamsDbcEntry> Entries;
    TMap<uint32, int32> IdIndex;
};
