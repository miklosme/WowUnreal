#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "WowTerrainTile.generated.h"

struct FAdtData;
class FMpqManager;
class FWowAssetCache;

UCLASS()
class WOWWORLD_API AWowTerrainTile : public AActor
{
    GENERATED_BODY()
public:
    AWowTerrainTile();
    void BuildFromAdtData(const FAdtData& Data, int32 TX, int32 TY, FMpqManager* Mpq, FWowAssetCache* Cache, TSet<uint32>* SpawnedWmoIds = nullptr);
    FIntPoint GetTileCoord() const { return TileCoord; }
private:
    UPROPERTY()
    TObjectPtr<USceneComponent> RootScene;

    UPROPERTY()
    TArray<TObjectPtr<UProceduralMeshComponent>> ChunkMeshes;

    FIntPoint TileCoord;

    UMaterialInterface* GetDefaultTerrainMaterial() const;
};
