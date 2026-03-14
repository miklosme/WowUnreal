#pragma once

#include "CoreMinimal.h"
#include "Formats/DbcParser.h"

struct FGroundEffectTextureDbcEntry
{
	uint32 ID;
	uint32 DoodadID[4];
	int32 DoodadWeight[4];
	uint32 Density;
	uint32 TerrainTypeID;
};

class WOWDATA_API FGroundEffectTextureDbc
{
public:
	bool Load(const FDbcParser& Parser);
	const FGroundEffectTextureDbcEntry* GetById(uint32 ID) const;
	const TArray<FGroundEffectTextureDbcEntry>& GetAll() const { return Entries; }
	int32 Num() const { return Entries.Num(); }

private:
	TArray<FGroundEffectTextureDbcEntry> Entries;
	TMap<uint32, int32> IdIndex;
};