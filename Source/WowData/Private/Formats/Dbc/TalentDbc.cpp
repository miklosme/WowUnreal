#include "Formats/Dbc/TalentDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogTalentDbc, Log, All);

bool FTalentDbc::Load(const FDbcParser& Parser)
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
		FTalentDbcEntry E;
		E.TalentID = Parser.GetUInt(i, 0);
		E.TalentTab = Parser.GetUInt(i, 1);
		E.Row = Parser.GetUInt(i, 2);
		E.Col = Parser.GetUInt(i, 3);
		E.RankID[0] = Parser.GetUInt(i, 4);
		E.RankID[1] = Parser.GetUInt(i, 5);
		E.RankID[2] = Parser.GetUInt(i, 6);
		E.RankID[3] = Parser.GetUInt(i, 7);
		E.RankID[4] = Parser.GetUInt(i, 8);
		E.DependsOn = Parser.GetUInt(i, 13);
		E.DependsOnRank = Parser.GetUInt(i, 16);
		E.Flags = Parser.GetUInt(i, 19);

		IdIndex.Add(E.TalentID, Entries.Num());
		Entries.Add(MoveTemp(E));
	}

	UE_LOG(LogTalentDbc, Log, TEXT("Loaded Talent.dbc: %d records"), Entries.Num());
	return true;
}

const FTalentDbcEntry* FTalentDbc::GetById(uint32 ID) const
{
	const int32* Idx = IdIndex.Find(ID);
	return Idx ? &Entries[*Idx] : nullptr;
}