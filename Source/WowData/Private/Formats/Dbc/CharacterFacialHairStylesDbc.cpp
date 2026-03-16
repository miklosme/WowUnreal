#include "Formats/Dbc/CharacterFacialHairStylesDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterFacialHairStylesDbc, Log, All);

bool FCharacterFacialHairStylesDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    RaceGenderVariationIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FCharacterFacialHairStylesDbcEntry E;
        E.RaceID = Parser.GetUInt(i, 0);         // First field is RaceID (no ID column)
        E.SexID = Parser.GetUInt(i, 1);
        E.VariationID = Parser.GetUInt(i, 2);
        E.Geosets[0] = Parser.GetUInt(i, 3);     // Geoset0
        E.Geosets[1] = Parser.GetUInt(i, 4);     // Geoset1
        E.Geosets[2] = Parser.GetUInt(i, 5);     // Geoset2
        E.Geosets[3] = Parser.GetUInt(i, 6);     // Geoset3
        E.Geosets[4] = Parser.GetUInt(i, 7);     // Geoset4

        // Create combined key for race/gender/variation lookup
        uint64 CombinedKey = (static_cast<uint64>(E.RaceID) << 32) |
                            (static_cast<uint64>(E.SexID) << 16) |
                            static_cast<uint64>(E.VariationID);
        RaceGenderVariationIndex.Add(CombinedKey, Entries.Num());

        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogCharacterFacialHairStylesDbc, Log, TEXT("Loaded CharacterFacialHairStyles.dbc: %d records"), Entries.Num());
    return true;
}

const FCharacterFacialHairStylesDbcEntry* FCharacterFacialHairStylesDbc::GetByRaceGenderVariation(uint32 RaceId, uint32 Gender, uint32 Variation) const
{
    uint64 CombinedKey = (static_cast<uint64>(RaceId) << 32) |
                        (static_cast<uint64>(Gender) << 16) |
                        static_cast<uint64>(Variation);
    const int32* Idx = RaceGenderVariationIndex.Find(CombinedKey);
    return Idx ? &Entries[*Idx] : nullptr;
}