#include "Formats/Dbc/SpellIconDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpellIconDbc, Log, All);

bool FSpellIconDbc::Load(const FDbcParser& Parser)
{
	if (!Parser.IsValid())
	{
		return false;
	}

	Entries.Reset();
	IdIndex.Reset();
	Entries.Reserve(Parser.GetRecordCount());

	for (uint32 i = 0; i < Parser.GetRecordCount(); ++i)
	{
		FSpellIconDbcEntry E;
		E.ID = Parser.GetUInt(i, 0);
		E.TexturePath = Parser.GetString(i, 1);

		IdIndex.Add(E.ID, Entries.Num());
		Entries.Add(MoveTemp(E));
	}

	UE_LOG(LogSpellIconDbc, Log, TEXT("Loaded SpellIcon.dbc: %d records"), Entries.Num());
	return true;
}

const FSpellIconDbcEntry* FSpellIconDbc::GetById(uint32 ID) const
{
	const int32* Idx = IdIndex.Find(ID);
	return Idx ? &Entries[*Idx] : nullptr;
}