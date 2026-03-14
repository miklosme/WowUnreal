#pragma once
#include "CoreMinimal.h"

class FMpqManager;
class FWowAssetCache;
struct FAdtWmoPlacement;

/**
 * Renders WMO (World Map Object) buildings using ProceduralMeshComponent.
 * Each WMO group becomes a separate ProceduralMeshComponent section.
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
};
