#include "Formats/Dbc/SpellVisualEffectNameDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpellVisualEffectNameDbc, Log, All);

bool FSpellVisualEffectNameDbc::Load(const FDbcParser& Parser)
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
		FSpellVisualEffectNameDbcEntry E;
		E.ID = Parser.GetUInt(i, 0);
		E.Name = Parser.GetString(i, 1);
		E.FilePath = Parser.GetString(i, 2);
		E.AreaEffectSize = Parser.GetFloat(i, 3);
		E.Scale = Parser.GetFloat(i, 4);

		// Convert .mdl/.mdx extensions to .m2 for compatibility
		if (E.Name.EndsWith(TEXT(".mdl")) || E.Name.EndsWith(TEXT(".mdx")))
		{
			E.Name = E.Name.LeftChop(4) + TEXT(".m2");
		}
		if (E.FilePath.EndsWith(TEXT(".mdl")) || E.FilePath.EndsWith(TEXT(".mdx")))
		{
			E.FilePath = E.FilePath.LeftChop(4) + TEXT(".m2");
		}

		IdIndex.Add(E.ID, Entries.Num());
		Entries.Add(MoveTemp(E));
	}

	UE_LOG(LogSpellVisualEffectNameDbc, Log, TEXT("Loaded SpellVisualEffectName.dbc: %d records"), Entries.Num());
	return true;
}

const FSpellVisualEffectNameDbcEntry* FSpellVisualEffectNameDbc::GetById(uint32 ID) const
{
	const int32* Idx = IdIndex.Find(ID);
	return Idx ? &Entries[*Idx] : nullptr;
}