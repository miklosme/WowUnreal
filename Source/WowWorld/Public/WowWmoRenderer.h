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

private:
    /** Construct group filename from root WMO path and group index */
    static FString GetGroupPath(const FString& RootPath, int32 GroupIndex);
};
