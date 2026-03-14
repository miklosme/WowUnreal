#pragma once

#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FTalentDbcEntry
{
	uint32 TalentID;
	uint32 TalentTab;
	uint32 Row;
	uint32 Col;
	uint32 RankID[5];
	uint32 DependsOn;
	uint32 DependsOnRank;
	uint32 Flags;
};

class WOWDATA_API FTalentDbc
{
public:
	bool Load(const FDbcParser& Parser);
	const FTalentDbcEntry* GetById(uint32 ID) const;
	const TArray<FTalentDbcEntry>& GetAll() const { return Entries; }
	int32 Num() const { return Entries.Num(); }

private:
	TArray<FTalentDbcEntry> Entries;
	TMap<uint32, int32> IdIndex;
};