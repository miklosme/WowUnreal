#include "Formats/Dbc/CharComponentTextureSectionsDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharComponentTextureSectionsDbc, Log, All);

bool FCharComponentTextureSectionsDbc::Load(const FDbcParser& Parser)
{
    if (!Parser.IsValid()) return false;

    Entries.Reset();
    IdIndex.Reset();
    LayoutIdIndex.Reset();
    Entries.Reserve(Parser.GetRecordCount());

    for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
    {
        FCharComponentTextureSectionsDbcEntry E;
        E.ID = Parser.GetUInt(i, 0);
        E.CharComponentTextureLayoutID = Parser.GetUInt(i, 1);
        E.SectionType = Parser.GetUInt(i, 2);
        E.X = Parser.GetUInt(i, 3);
        E.Y = Parser.GetUInt(i, 4);
        E.Width = Parser.GetUInt(i, 5);
        E.Height = Parser.GetUInt(i, 6);

        IdIndex.Add(E.ID, Entries.Num());
        LayoutIdIndex.Add(E.CharComponentTextureLayoutID, Entries.Num());
        Entries.Add(MoveTemp(E));
    }

    UE_LOG(LogCharComponentTextureSectionsDbc, Log, TEXT("Loaded CharComponentTextureSections.dbc: %d records"), Entries.Num());
    return true;
}

const FCharComponentTextureSectionsDbcEntry* FCharComponentTextureSectionsDbc::GetById(uint32 ID) const
{
    const int32* Idx = IdIndex.Find(ID);
    return Idx ? &Entries[*Idx] : nullptr;
}

TArray<const FCharComponentTextureSectionsDbcEntry*> FCharComponentTextureSectionsDbc::GetByLayoutId(uint32 LayoutID) const
{
    TArray<const FCharComponentTextureSectionsDbcEntry*> Result;

    TArray<int32> Indices;
    LayoutIdIndex.MultiFind(LayoutID, Indices);

    Result.Reserve(Indices.Num());
    for (int32 Idx : Indices)
    {
        if (Entries.IsValidIndex(Idx))
        {
            Result.Add(&Entries[Idx]);
        }
    }

    return Result;
}