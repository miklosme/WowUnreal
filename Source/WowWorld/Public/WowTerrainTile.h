#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Formats/AdtTypes.h"
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

    // ---- Distance-based streaming data ----
    // Parsed placement data stored for deferred spawning
    TArray<FAdtDoodadPlacement> DoodadPlacements;
    TArray<FString> DoodadPaths;
    TArray<FAdtWmoPlacement> WmoPlacements;
    TArray<FString> WmoPaths;

    // Track which placements are currently spawned
    UPROPERTY()
    TMap<uint32, TObjectPtr<UProceduralMeshComponent>> SpawnedDoodads; // UniqueId -> component

    UPROPERTY()
    TMap<uint32, TObjectPtr<AActor>> SpawnedWmos; // UniqueId -> actor

    // Cached pointers for deferred spawning
    FMpqManager* CachedMpq = nullptr;
    FWowAssetCache* CachedCache = nullptr;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> RootScene;

    UPROPERTY()
    TArray<TObjectPtr<UProceduralMeshComponent>> ChunkMeshes;

    FIntPoint TileCoord;

    UMaterialInterface* GetDefaultTerrainMaterial() const;
};
