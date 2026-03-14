#include "Formats/Dbc/CharSectionsDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharSectionsDbc, Log, All);

bool FCharSectionsDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    IdIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FCharSectionsDbcEntry E;
        E.ID = Parser.GetUInt(i, 0);
        E.RaceID = Parser.GetUInt(i, 1);
        E.SexID = Parser.GetUInt(i, 2);
        E.Variation = Parser.GetUInt(i, 3);
        E.Textures[0] = Parser.GetString(i, 4);
        E.Textures[1] = Parser.GetString(i, 5);
        E.Textures[2] = Parser.GetString(i, 6);
        E.Flags = Parser.GetUInt(i, 7);
        E.Type = Parser.GetUInt(i, 8);
        E.Color = Parser.GetUInt(i, 9);

        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogCharSectionsDbc, Log, TEXT("Loaded CharSections.dbc: %d records"), Entries.Num());
    return true;
}

const FCharSectionsDbcEntry* FCharSectionsDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}
