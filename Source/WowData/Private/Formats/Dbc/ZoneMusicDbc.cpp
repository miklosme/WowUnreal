#include "Formats/Dbc/ZoneMusicDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogZoneMusicDbc, Log, All);

bool FZoneMusicDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    IdIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FZoneMusicDbcEntry E;
        E.ID = Parser.GetUInt(i, 0);
        E.Name = Parser.GetString(i, 1);
        // Columns 2-5 are silence intervals (min/max day/night) — skip
        E.SoundDayID = Parser.GetUInt(i, 6);
        E.SoundNightID = Parser.GetUInt(i, 7);

        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogZoneMusicDbc, Log, TEXT("Loaded ZoneMusic.dbc: %d records"), Entries.Num());
    return true;
}

const FZoneMusicDbcEntry* FZoneMusicDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}
