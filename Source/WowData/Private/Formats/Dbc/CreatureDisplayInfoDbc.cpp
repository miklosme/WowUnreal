#include "Formats/Dbc/CreatureDisplayInfoDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogCreatureDisplayInfoDbc, Log, All);

bool FCreatureDisplayInfoDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    IdIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FCreatureDisplayInfoDbcEntry E;
        E.ID = Parser.GetUInt(i, 0);
        E.ModelID = Parser.GetUInt(i, 1);
        E.SoundID = Parser.GetUInt(i, 2);
        E.ExtendedDisplayInfoID = Parser.GetUInt(i, 3);
        E.Scale = Parser.GetFloat(i, 4);
        E.Opacity = Parser.GetUInt(i, 5);
        E.Texture1 = Parser.GetString(i, 6);
        E.Texture2 = Parser.GetString(i, 7);
        E.Texture3 = Parser.GetString(i, 8);
        E.PortraitTextureName = Parser.GetString(i, 9);
        E.BloodLevel = Parser.GetUInt(i, 10);
        E.BloodID = Parser.GetUInt(i, 11);
        E.NPCSoundID = Parser.GetUInt(i, 12);
        E.ParticleColorID = Parser.GetUInt(i, 13);
        E.CreatureGeosetData = Parser.GetUInt(i, 14);
        E.ObjectEffectPackageID = Parser.GetUInt(i, 15);

        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogCreatureDisplayInfoDbc, Log, TEXT("Loaded CreatureDisplayInfo.dbc: %d records"), Entries.Num());
    return true;
}

const FCreatureDisplayInfoDbcEntry* FCreatureDisplayInfoDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}
