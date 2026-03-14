#include "Formats/Dbc/SoundEntriesDbc.h"

DEFINE_LOG_CATEGORY_STATIC(LogSoundEntriesDbc, Log, All);

bool FSoundEntriesDbc::Load(const FDbcParser& Parser)
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
		FSoundEntriesDbcEntry E;
		E.ID = Parser.GetUInt(i, 0);
		E.SoundType = Parser.GetUInt(i, 1);
		E.Name = Parser.GetString(i, 2);

		// FileDataID[10] - columns 3-12 are strings (file names)
		for (int32 j = 0; j < 10; ++j)
		{
			E.FileDataID[j] = Parser.GetString(i, 3 + j);
		}

		// Freq[10] - columns 13-22
		for (int32 j = 0; j < 10; ++j)
		{
			E.Freq[j] = Parser.GetUInt(i, 13 + j);
		}

		E.DirectoryBase = Parser.GetString(i, 23);
		E.VolumeFloat = Parser.GetFloat(i, 24);
		E.Flags = Parser.GetUInt(i, 25);
		E.MinDistance = Parser.GetFloat(i, 26);
		E.DistanceCutoff = Parser.GetFloat(i, 27);
		E.EAXDef = Parser.GetUInt(i, 28);
		E.SoundEntriesAdvancedID = Parser.GetUInt(i, 29);

		IdIndex.Add(E.ID, Entries.Num());
		Entries.Add(MoveTemp(E));
	}

	UE_LOG(LogSoundEntriesDbc, Log, TEXT("Loaded SoundEntries.dbc: %d records"), Entries.Num());
	return true;
}

const FSoundEntriesDbcEntry* FSoundEntriesDbc::GetById(uint32 ID) const
{
	const int32* Idx = IdIndex.Find(ID);
	return Idx ? &Entries[*Idx] : nullptr;
}