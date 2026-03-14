#pragma once
#include "CoreMinimal.h"

class UProceduralMeshComponent;
struct FAdtData;

class WOWWORLD_API FWowWaterRenderer
{
public:
	/** Create water mesh components for all water chunks in an ADT, attach to Owner */
	static TArray<UProceduralMeshComponent*> CreateWaterMeshes(
		AActor* Owner, const FAdtData& AdtData, int32 TileX, int32 TileY);
};
