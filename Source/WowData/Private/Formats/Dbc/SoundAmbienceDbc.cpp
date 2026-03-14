#include "Formats/Dbc/SoundAmbienceDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoundAmbienceDbc, Log, All);

bool FSoundAmbienceDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    IdIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FSoundAmbienceDbcEntry E;
        E.ID = Parser.GetUInt(i, 0);
        E.DayAmbience = Parser.GetUInt(i, 1);
        E.NightAmbience = Parser.GetUInt(i, 2);

        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogSoundAmbienceDbc, Log, TEXT("Loaded SoundAmbience.dbc: %d records"), Entries.Num());
    return true;
}

const FSoundAmbienceDbcEntry* FSoundAmbienceDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}
