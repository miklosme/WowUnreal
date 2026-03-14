#pragma once

#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FTalentTabDbcEntry
{
	uint32 TalentTabID;
	FString Name;
	uint32 NameFlags;
	uint32 SpellIcon;
	uint32 ClassMask;
	uint32 PetTalentMask;
	uint32 TabPage;
	FString InternalName;
};

class WOWDATA_API FTalentTabDbc
{
public:
	bool Load(const FDbcParser& Parser);
	const FTalentTabDbcEntry* GetById(uint32 ID) const;
	const TArray<FTalentTabDbcEntry>& GetAll() const { return Entries; }
	int32 Num() const { return Entries.Num(); }

private:
	TArray<FTalentTabDbcEntry> Entries;
	TMap<uint32, int32> IdIndex;
};