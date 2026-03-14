#pragma once
#include "CoreMinimal.h"
#include "Formats/Dbc/MapDbc.h"
#include "Formats/Dbc/AreaTableDbc.h"
#include "Formats/Dbc/LightDbc.h"
#include "Formats/Dbc/LightParamsDbc.h"
#include "Formats/Dbc/LightIntParamsDbc.h"
#include "Formats/Dbc/LiquidTypeDbc.h"
#include "Formats/Dbc/AnimationDataDbc.h"
#include "Formats/Dbc/ChrRacesDbc.h"
#include "Formats/Dbc/CharSectionsDbc.h"
#include "Formats/Dbc/CreatureDisplayInfoDbc.h"
#include "Formats/Dbc/CreatureModelDataDbc.h"
#include "Formats/Dbc/ItemDisplayInfoDbc.h"

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
    FLightIntParamsDbc& LightIntParams() { return LightIntParamsDbc; }
    FLiquidTypeDbc& LiquidTypes() { return LiquidTypeDbc; }
    FAnimationDataDbc& AnimationData() { return AnimationDataDbc; }
    FChrRacesDbc& ChrRaces() { return ChrRacesDbc; }
    FCharSectionsDbc& CharSections() { return CharSectionsDbc; }
    FCreatureDisplayInfoDbc& CreatureDisplayInfo() { return CreatureDisplayInfoDbc; }
    FCreatureModelDataDbc& CreatureModelData() { return CreatureModelDataDbc; }
    FItemDisplayInfoDbc& ItemDisplayInfo() { return ItemDisplayInfoDbc; }

    const FMapDbc& Maps() const { return MapDbc; }
    const FAreaTableDbc& AreaTable() const { return AreaTableDbc; }
    const FLightDbc& Lights() const { return LightDbc; }
    const FLightParamsDbc& LightParams() const { return LightParamsDbc; }
    const FLightIntParamsDbc& LightIntParams() const { return LightIntParamsDbc; }
    const FLiquidTypeDbc& LiquidTypes() const { return LiquidTypeDbc; }
    const FAnimationDataDbc& AnimationData() const { return AnimationDataDbc; }
    const FChrRacesDbc& ChrRaces() const { return ChrRacesDbc; }
    const FCharSectionsDbc& CharSections() const { return CharSectionsDbc; }
    const FCreatureDisplayInfoDbc& CreatureDisplayInfo() const { return CreatureDisplayInfoDbc; }
    const FCreatureModelDataDbc& CreatureModelData() const { return CreatureModelDataDbc; }
    const FItemDisplayInfoDbc& ItemDisplayInfo() const { return ItemDisplayInfoDbc; }

    bool IsLoaded() const { return bLoaded; }

private:
    FMapDbc MapDbc;
    FAreaTableDbc AreaTableDbc;
    FLightDbc LightDbc;
    FLightParamsDbc LightParamsDbc;
    FLightIntParamsDbc LightIntParamsDbc;
    FLiquidTypeDbc LiquidTypeDbc;
    FAnimationDataDbc AnimationDataDbc;
    FChrRacesDbc ChrRacesDbc;
    FCharSectionsDbc CharSectionsDbc;
    FCreatureDisplayInfoDbc CreatureDisplayInfoDbc;
    FCreatureModelDataDbc CreatureModelDataDbc;
    FItemDisplayInfoDbc ItemDisplayInfoDbc;
    bool bLoaded = false;
};
