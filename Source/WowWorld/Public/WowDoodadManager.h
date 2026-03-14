#pragma once
#include "CoreMinimal.h"

class FMpqManager;
class FWowAssetCache;
class UProceduralMeshComponent;
struct FAdtDoodadPlacement;
struct FM2Data;

/**
 * Manages M2 doodad placement using ProceduralMeshComponent.
 * Creates one ProceduralMeshComponent per doodad instance.
 */
class WOWWORLD_API FWowDoodadManager
{
public:
    /** Parse M2 data from MPQ (cached). Returns shared pointer to parsed data. */
    static TSharedPtr<FM2Data> GetOrParseM2(const FString& M2Path, FMpqManager* Mpq);

    /** Spawn doodad instances for a tile as ProceduralMeshComponents on the parent actor */
    static void SpawnDoodads(AActor* ParentActor, const TArray<FAdtDoodadPlacement>& Placements,
                             const TArray<FString>& DoodadPaths, FMpqManager* Mpq, FWowAssetCache* Cache);

    /** Spawn a single doodad and return the ProceduralMeshComponent (for distance-based streaming) */
    static UProceduralMeshComponent* SpawnSingleDoodad(AActor* ParentActor, const FAdtDoodadPlacement& Placement,
                                                        const FString& M2Path, FMpqManager* Mpq, FWowAssetCache* Cache);

private:
    /** Cache of parsed M2 data (not UObjects, just geometry) */
    static TMap<FString, TSharedPtr<FM2Data>> ParsedM2Cache;
    static FCriticalSection CacheLock;

    /** Build skin file path from M2 path */
    static FString GetSkinPath(const FString& M2Path);

    /** Create a ProceduralMeshComponent from M2 data */
    static UProceduralMeshComponent* CreateM2MeshComponent(AActor* Owner, const FM2Data& Data,
                                                            const FString& M2Path, FMpqManager* Mpq,
                                                            FWowAssetCache* Cache, FName CompName);
};
