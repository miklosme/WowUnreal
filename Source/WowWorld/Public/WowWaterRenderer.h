#pragma once
#include "CoreMinimal.h"

class UStaticMeshComponent;
struct FAdtData;

class WOWWORLD_API FWowWaterRenderer
{
public:
	/** Create water mesh components for all water chunks in an ADT, attach to Owner */
	static TArray<UStaticMeshComponent*> CreateWaterMeshes(
		AActor* Owner, const FAdtData& AdtData, int32 TileX, int32 TileY);

	/** Create a large ocean plane at the given height, centered on the tile */
	static UStaticMeshComponent* CreateOceanPlane(
		AActor* Owner, float OceanHeight, int32 TileX, int32 TileY);
};
