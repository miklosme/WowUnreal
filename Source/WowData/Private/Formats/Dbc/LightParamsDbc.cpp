#include "Formats/Dbc/LightParamsDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogLightParamsDbc, Log, All);

bool FLightParamsDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reserve(Parser.GetRecordCount());
    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FLightParamsDbcEntry E;
        E.ID                = Parser.GetUInt(i, 0);
        E.HighlightSky      = Parser.GetUInt(i, 1);
        E.SkyboxID          = Parser.GetUInt(i, 2);
        E.CloudTypeID       = Parser.GetUInt(i, 3);
        E.Glow              = Parser.GetFloat(i, 4);
        E.WaterShallowAlpha = Parser.GetFloat(i, 5);
        E.WaterDeepAlpha    = Parser.GetFloat(i, 6);
        E.OceanShallowAlpha = Parser.GetFloat(i, 7);
        E.OceanDeepAlpha    = Parser.GetFloat(i, 8);
        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogLightParamsDbc, Log, TEXT("Loaded LightParams.dbc: %d records"), Entries.Num());
    return true;
}

const FLightParamsDbcEntry* FLightParamsDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}
