#include "Formats/Dbc/LoadingScreensDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogLoadingScreensDbc, Log, All);

bool FLoadingScreensDbc::Load(const FDbcParser& Parser)
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
		FLoadingScreensDbcEntry E;
		E.ID = Parser.GetUInt(i, 0);
		E.Name = Parser.GetString(i, 1);
		E.FileName = Parser.GetString(i, 2);
		E.HasWideScreen = Parser.GetUInt(i, 3);

		IdIndex.Add(E.ID, Entries.Num());
		Entries.Add(MoveTemp(E));
	}

	UE_LOG(LogLoadingScreensDbc, Log, TEXT("Loaded LoadingScreens.dbc: %d records"), Entries.Num());
	return true;
}

const FLoadingScreensDbcEntry* FLoadingScreensDbc::GetById(uint32 ID) const
{
	const int32* Idx = IdIndex.Find(ID);
	return Idx ? &Entries[*Idx] : nullptr;
}