#include "Formats/Dbc/LightIntParamsDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogLightIntParamsDbc, Log, All);

bool FLightIntParamsDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    IdIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FLightIntParamsDbcEntry E;
        E.ID = Parser.GetUInt(i, 0);
        E.EntryCount = Parser.GetUInt(i, 1);
        for (int32 j = 0; j < FLightIntParamsDbcEntry::MaxEntries; ++j)
        {
            E.Times[j] = Parser.GetUInt(i, 2 + j);
            E.Values[j] = Parser.GetUInt(i, 18 + j);
        }

        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogLightIntParamsDbc, Log, TEXT("Loaded LightIntBand.dbc: %d records"), Entries.Num());
    return true;
}

const FLightIntParamsDbcEntry* FLightIntParamsDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}
