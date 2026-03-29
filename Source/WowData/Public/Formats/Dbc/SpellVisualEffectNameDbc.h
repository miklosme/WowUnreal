#pragma once

#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FSpellVisualEffectNameDbcEntry
{
	uint32 ID;
	FString Name;
	FString FilePath;
	float AreaEffectSize;
	float Scale;
};

class WOWDATA_API FSpellVisualEffectNameDbc
{
public:
	bool Load(const FDbcParser& Parser);
	const FSpellVisualEffectNameDbcEntry* GetById(uint32 ID) const;
	const TArray<FSpellVisualEffectNameDbcEntry>& GetAll() const { return Entries; }
	int32 Num() const { return Entries.Num(); }

private:
	TArray<FSpellVisualEffectNameDbcEntry> Entries;
	TMap<uint32, int32> IdIndex;
};