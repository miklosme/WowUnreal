#pragma once
#include "CoreMinimal.h"
#include "Formats/Dbc/MapDbc.h"
#include "Formats/Dbc/AreaTableDbc.h"
#include "Formats/Dbc/LightDbc.h"
#include "Formats/Dbc/LightParamsDbc.h"

class FMpqManager;

class WOWDATA_API FDbcStore
{
public:
    static FDbcStore& Get();

    bool LoadAll(FMpqManager& Mpq);

    FMapDbc& Maps() { return MapDbc; }
    FAreaTableDbc& AreaTable() { return AreaTableDbc; }
    FLightDbc& Lights() { return LightDbc; }
    FLightParamsDbc& LightParams() { return LightParamsDbc; }

    const FMapDbc& Maps() const { return MapDbc; }
    const FAreaTableDbc& AreaTable() const { return AreaTableDbc; }
    const FLightDbc& Lights() const { return LightDbc; }
    const FLightParamsDbc& LightParams() const { return LightParamsDbc; }

    bool IsLoaded() const { return bLoaded; }

private:
    FMapDbc MapDbc;
    FAreaTableDbc AreaTableDbc;
    FLightDbc LightDbc;
    FLightParamsDbc LightParamsDbc;
    bool bLoaded = false;
};
