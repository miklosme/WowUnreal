#pragma once

#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FLoadingScreensDbcEntry
{
	uint32 ID;
	FString Name;
	FString FileName;
	uint32 HasWideScreen;
};

class WOWDATA_API FLoadingScreensDbc
{
public:
	bool Load(const FDbcParser& Parser);
	const FLoadingScreensDbcEntry* GetById(uint32 ID) const;
	const TArray<FLoadingScreensDbcEntry>& GetAll() const { return Entries; }
	int32 Num() const { return Entries.Num(); }

private:
	TArray<FLoadingScreensDbcEntry> Entries;
	TMap<uint32, int32> IdIndex;
};