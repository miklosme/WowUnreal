#pragma once
#include "CoreMinimal.h"

class FMpqManager;
class FWowAssetCache;
class UStaticMesh;
class USkeletalMesh;
class USkeleton;
class UAnimSequence;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
struct FAdtDoodadPlacement;
struct FM2Data;

/**
 * Manages M2 doodad placement.
 * Supports both UStaticMeshComponent (per-instance) and HISMC (instanced) modes.
 */
class WOWWORLD_API FWowDoodadManager
{
public:
    /** Parse M2 data from MPQ (cached). Returns shared pointer to parsed data. */
    static TSharedPtr<FM2Data> GetOrParseM2(const FString& M2Path, FMpqManager* Mpq);

    /** Spawn doodad instances for a tile as UStaticMeshComponents on the parent actor */
    static void SpawnDoodads(AActor* ParentActor, const TArray<FAdtDoodadPlacement>& Placements,
                             const TArray<FString>& DoodadPaths, FMpqManager* Mpq, FWowAssetCache* Cache);

    /** Spawn a single doodad and return the UStaticMeshComponent (for distance-based streaming) */
    static UStaticMeshComponent* SpawnSingleDoodad(AActor* ParentActor, const FAdtDoodadPlacement& Placement,
                                                    const FString& M2Path, FMpqManager* Mpq, FWowAssetCache* Cache);

    /**
     * Spawn all doodads for a tile using HISMC instancing.
     * Groups placements by M2 model and creates one HISMC per unique model.
     * Returns the created HISMC components (caller owns them).
     */
    static TArray<UHierarchicalInstancedStaticMeshComponent*> SpawnDoodadsInstanced(
        AActor* ParentActor, const TArray<FAdtDoodadPlacement>& Placements,
        const TArray<FString>& DoodadPaths, FMpqManager* Mpq, FWowAssetCache* Cache);

    /** Get or create a UStaticMesh from M2 data (cached in asset cache) */
    static UStaticMesh* GetOrCreateStaticMesh(const FString& M2Path, FMpqManager* Mpq, FWowAssetCache* Cache);

    /** Check if an M2 model should use skeletal mesh (has bones + animation data) */
    static bool ShouldUseSkeletalMesh(const FM2Data& Data);

    /** Get or create a USkeletalMesh from animated M2 data (cached in asset cache) */
    static USkeletalMesh* GetOrCreateSkeletalMesh(const FString& M2Path, FMpqManager* Mpq, FWowAssetCache* Cache);

private:
    /** Cache of parsed M2 data (not UObjects, just geometry) */
    static TMap<FString, TSharedPtr<FM2Data>> ParsedM2Cache;
    static FCriticalSection CacheLock;

    /** Cache of skeletons per M2 model (shared between mesh and animations) */
    static TMap<FString, TWeakObjectPtr<USkeleton>> SkeletonCache;

    /** Cache of animation sequences per M2 model */
    static TMap<FString, TArray<TWeakObjectPtr<UAnimSequence>>> AnimCache;

    /** Build skin file path from M2 path */
    static FString GetSkinPath(const FString& M2Path);

    /** Create a UStaticMesh from M2 geometry data */
    static UStaticMesh* CreateStaticMeshFromM2(const FM2Data& Data, const FString& M2Path,
                                                FMpqManager* Mpq, FWowAssetCache* Cache);
};
