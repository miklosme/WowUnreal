#include "Formats/Dbc/MapDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogMapDbc, Log, All);

bool FMapDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reserve(Parser.GetRecordCount());
    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FMapDbcEntry E;
        E.ID           = Parser.GetUInt(i, 0);
        E.InternalName = Parser.GetString(i, 1);
        E.MapType      = Parser.GetUInt(i, 2);
        E.Flags        = Parser.GetUInt(i, 3);
        E.Name         = Parser.GetString(i, 5);
        E.LoadingScreenID = Parser.GetUInt(i, 21);
        E.LinkedZone   = Parser.GetUInt(i, 22);
        E.ExpansionID  = Parser.GetUInt(i, 63);
        E.MaxPlayers   = Parser.GetUInt(i, 65);
        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogMapDbc, Log, TEXT("Loaded Map.dbc: %d records"), Entries.Num());
    return true;
}

const FMapDbcEntry* FMapDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}
