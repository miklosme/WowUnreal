#pragma once

#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FSpellIconDbcEntry
{
	uint32 ID;
	FString TexturePath;
};

class WOWDATA_API FSpellIconDbc
{
public:
	bool Load(const FDbcParser& Parser);
	const FSpellIconDbcEntry* GetById(uint32 ID) const;
	const TArray<FSpellIconDbcEntry>& GetAll() const { return Entries; }
	int32 Num() const { return Entries.Num(); }

private:
	TArray<FSpellIconDbcEntry> Entries;
	TMap<uint32, int32> IdIndex;
};