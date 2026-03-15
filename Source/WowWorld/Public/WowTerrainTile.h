#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Formats/AdtTypes.h"
#include "WowTerrainTile.generated.h"

struct FAdtData;
class FMpqManager;
class FWowAssetCache;
class UInstancedStaticMeshComponent;
class URuntimeVirtualTexture;
class UStaticMeshComponent;

UCLASS()
class WOWWORLD_API AWowTerrainTile : public AActor
{
    GENERATED_BODY()
public:
    AWowTerrainTile();
    void BuildFromAdtData(const FAdtData& Data, int32 TX, int32 TY, FMpqManager* Mpq, FWowAssetCache* Cache, TSet<uint32>* SpawnedWmoIds = nullptr);
    void ApplyRuntimeVirtualTexture(URuntimeVirtualTexture* RVT);
    FIntPoint GetTileCoord() const { return TileCoord; }

    // ---- Distance-based streaming data ----
    // Parsed placement data stored for deferred spawning
    TArray<FAdtDoodadPlacement> DoodadPlacements;
    TArray<FString> DoodadPaths;
    TArray<FAdtWmoPlacement> WmoPlacements;
    TArray<FString> WmoPaths;

    // Track which placements are currently spawned (legacy per-instance mode)
    UPROPERTY()
    TMap<uint32, TObjectPtr<UStaticMeshComponent>> SpawnedDoodads; // UniqueId -> component

    UPROPERTY()
    TMap<uint32, TObjectPtr<AActor>> SpawnedWmos; // UniqueId -> actor

    /** HISMC components for instanced doodads (one per unique M2 model) */
    UPROPERTY()
    TArray<TObjectPtr<UInstancedStaticMeshComponent>> InstancedDoodads;

    /** Whether this tile's doodads are using HISMC instancing */
    bool bUsesInstancedDoodads = false;

    // Cached pointers for deferred spawning
    FMpqManager* CachedMpq = nullptr;
    FWowAssetCache* CachedCache = nullptr;

    /** Area IDs per chunk (16x16 = 256 entries, row-major: [chunkY * 16 + chunkX]) */
    TArray<uint32> ChunkAreaIds;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> RootScene;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> ChunkMeshes;

    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> WaterMeshes;

    FIntPoint TileCoord;

    UMaterialInterface* GetDefaultTerrainMaterial() const;
};
