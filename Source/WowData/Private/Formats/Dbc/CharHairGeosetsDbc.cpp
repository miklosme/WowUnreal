#include "Formats/Dbc/CharHairGeosetsDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharHairGeosetsDbc, Log, All);

bool FCharHairGeosetsDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    IdIndex.Reset();
    RaceGenderVariationIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FCharHairGeosetsDbcEntry E;
        E.ID = Parser.GetUInt(i, 0);
        E.RaceID = Parser.GetUInt(i, 1);
        E.SexID = Parser.GetUInt(i, 2);
        E.VariationID = Parser.GetUInt(i, 3);
        E.GeosetID = Parser.GetUInt(i, 4);
        E.Showscalp = Parser.GetUInt(i, 5);

        IdIndex.Add(E.ID, Entries.Num());

        // Create combined key for race/gender/variation lookup
        uint64 CombinedKey = (static_cast<uint64>(E.RaceID) << 32) |
                            (static_cast<uint64>(E.SexID) << 16) |
                            static_cast<uint64>(E.VariationID);
        RaceGenderVariationIndex.Add(CombinedKey, Entries.Num());

        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogCharHairGeosetsDbc, Log, TEXT("Loaded CharHairGeosets.dbc: %d records"), Entries.Num());
    return true;
}

const FCharHairGeosetsDbcEntry* FCharHairGeosetsDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}

const FCharHairGeosetsDbcEntry* FCharHairGeosetsDbc::GetByRaceGenderVariation(uint32 RaceId, uint32 Gender, uint32 Variation) const
{
    uint64 CombinedKey = (static_cast<uint64>(RaceId) << 32) |
                        (static_cast<uint64>(Gender) << 16) |
                        static_cast<uint64>(Variation);
    const int32* Idx = RaceGenderVariationIndex.Find(CombinedKey);
    return Idx ? &Entries[*Idx] : nullptr;
}