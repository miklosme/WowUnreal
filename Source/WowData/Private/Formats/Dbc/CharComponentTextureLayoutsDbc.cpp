#include "Formats/Dbc/CharComponentTextureLayoutsDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharComponentTextureLayoutsDbc, Log, All);

bool FCharComponentTextureLayoutsDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    IdIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FCharComponentTextureLayoutsDbcEntry E;
        E.ID = Parser.GetUInt(i, 0);
        E.Width = Parser.GetUInt(i, 1);
        E.Height = Parser.GetUInt(i, 2);

        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogCharComponentTextureLayoutsDbc, Log, TEXT("Loaded CharComponentTextureLayouts.dbc: %d records"), Entries.Num());
    return true;
}

const FCharComponentTextureLayoutsDbcEntry* FCharComponentTextureLayoutsDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}