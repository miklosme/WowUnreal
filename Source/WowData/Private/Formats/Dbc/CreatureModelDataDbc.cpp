#include "Formats/Dbc/CreatureModelDataDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogCreatureModelDataDbc, Log, All);

bool FCreatureModelDataDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    IdIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FCreatureModelDataDbcEntry E;
        E.ID = Parser.GetUInt(i, 0);
        E.Flags = Parser.GetUInt(i, 1);
        E.ModelPath = Parser.GetString(i, 2);
        E.SizeClass = Parser.GetUInt(i, 3);
        E.Scale = Parser.GetFloat(i, 4);
        E.BloodLevelID = Parser.GetUInt(i, 5);
        E.FootprintID = Parser.GetUInt(i, 6);
        E.FootprintLength = Parser.GetFloat(i, 7);
        E.FootprintWidth = Parser.GetFloat(i, 8);
        E.FootprintParticleScale = Parser.GetFloat(i, 9);
        E.FoleyMaterialID = Parser.GetUInt(i, 10);
        E.FootstepShakeSizeID = Parser.GetUInt(i, 11);
        E.DeathThudShakeSizeID = Parser.GetUInt(i, 12);
        E.SoundDataID = Parser.GetUInt(i, 13);
        E.CollisionWidth = Parser.GetFloat(i, 14);
        E.CollisionHeight = Parser.GetFloat(i, 15);
        E.MountHeight = Parser.GetFloat(i, 16);

        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogCreatureModelDataDbc, Log, TEXT("Loaded CreatureModelData.dbc: %d records"), Entries.Num());
    return true;
}

const FCreatureModelDataDbcEntry* FCreatureModelDataDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}
