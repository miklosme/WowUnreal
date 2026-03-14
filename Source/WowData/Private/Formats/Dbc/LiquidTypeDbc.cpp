#include "Formats/Dbc/LiquidTypeDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogLiquidTypeDbc, Log, All);

bool FLiquidTypeDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    IdIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FLiquidTypeDbcEntry E;
        E.ID = Parser.GetUInt(i, 0);
        E.Name = Parser.GetString(i, 1);
        E.Flags = Parser.GetUInt(i, 2);
        E.Type = Parser.GetUInt(i, 3);
        E.SoundID = Parser.GetUInt(i, 4);
        E.SpellID = Parser.GetUInt(i, 5);

        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogLiquidTypeDbc, Log, TEXT("Loaded LiquidType.dbc: %d records"), Entries.Num());
    return true;
}

const FLiquidTypeDbcEntry* FLiquidTypeDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}
