#pragma once
#include "CoreMinimal.h"

class FMpqManager;
class FWowAssetCache;
class UStaticMesh;
struct FAdtWmoPlacement;
struct FWmoGroupData;
struct FWmoRootData;

/**
 * Renders WMO (World Map Object) buildings.
 * Uses UStaticMesh with Nanite when supported.
 */
class WOWWORLD_API FWowWmoRenderer
{
public:
    /** Spawn a WMO instance at the given placement. Returns the parent actor. */
    static AActor* SpawnWmo(UWorld* World, const FString& WmoPath, const FAdtWmoPlacement& Placement,
                            FMpqManager* Mpq, FWowAssetCache* Cache);

    /** Query the number of groups in a WMO root file without spawning anything. Returns 0 on failure. */
    static uint32 GetWmoGroupCount(const FString& WmoPath, FMpqManager* Mpq);

private:
    /** Construct group filename from root WMO path and group index */
    static FString GetGroupPath(const FString& RootPath, int32 GroupIndex);

    /** Create a UStaticMesh from WMO group geometry with Nanite support */
    static UStaticMesh* CreateStaticMeshFromWmoGroup(const FWmoGroupData& GroupData,
        const FWmoRootData& RootData, const FString& WmoPath, int32 GroupIdx,
        FMpqManager* Mpq, FWowAssetCache* Cache);
};
