#pragma once

#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FSoundEntriesDbcEntry
{
	uint32 ID;
	uint32 SoundType;
	FString Name;
	FString FileDataID[10];  // File names
	uint32 Freq[10];
	FString DirectoryBase;
	float VolumeFloat;
	uint32 Flags;
	float MinDistance;
	float DistanceCutoff;
	uint32 EAXDef;
	uint32 SoundEntriesAdvancedID;
};

class WOWDATA_API FSoundEntriesDbc
{
public:
	bool Load(const FDbcParser& Parser);
	const FSoundEntriesDbcEntry* GetById(uint32 ID) const;
	const TArray<FSoundEntriesDbcEntry>& GetAll() const { return Entries; }
	int32 Num() const { return Entries.Num(); }

private:
	TArray<FSoundEntriesDbcEntry> Entries;
	TMap<uint32, int32> IdIndex;
};