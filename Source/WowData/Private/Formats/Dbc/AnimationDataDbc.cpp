#include "Formats/Dbc/AnimationDataDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogAnimationDataDbc, Log, All);

bool FAnimationDataDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    IdIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FAnimationDataDbcEntry E;
        E.ID = Parser.GetUInt(i, 0);
        E.Name = Parser.GetString(i, 1);
        E.WeaponFlags = Parser.GetUInt(i, 2);
        E.BodyFlags = Parser.GetUInt(i, 3);
        E.Flags = Parser.GetUInt(i, 4);
        E.Fallback = Parser.GetUInt(i, 5);
        E.BehaviorID = Parser.GetUInt(i, 6);
        E.BehaviorTier = Parser.GetUInt(i, 7);

        IdIndex.Add(E.ID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogAnimationDataDbc, Log, TEXT("Loaded AnimationData.dbc: %d records"), Entries.Num());
    return true;
}

const FAnimationDataDbcEntry* FAnimationDataDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}
