#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "Formats/WdtTypes.h"
#include "Formats/AdtTypes.h"
#include "Async/Future.h"
#include "WowWorldManager.generated.h"

class AWowTerrainTile;

/** Result of an async tile load (MPQ read + ADT parse on background thread) */
struct FPendingTileLoad
{
    int32 TX = 0;
    int32 TY = 0;
    TFuture<TSharedPtr<FAdtData>> Future;
};

UCLASS()
class WOWWORLD_API AWowWorldManager : public AActor
{
    GENERATED_BODY()
public:
    AWowWorldManager();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

    FMpqManager* GetMpqManager() const { return MpqManager.Get(); }
    FWowAssetCache* GetAssetCache() const { return AssetCache.Get(); }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW")
    FString MapName = TEXT("Azeroth");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 LoadRadius = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 UnloadRadius = 4;

    /** Initial tile to load for testing (Elwynn Forest area) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Debug")
    int32 DebugTileX = 32;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Debug")
    int32 DebugTileY = 48;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Debug")
    bool bStreamingEnabled = false;

    // ---- Object streaming settings ----
    /** Radius (UE cm) within which doodads (M2) are spawned. Default 2000m. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    float DoodadRadius = 200000.0f;

    /** Radius (UE cm) within which WMOs (buildings) are spawned. Default 3000m. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    float WmoRadius = 300000.0f;

    /** Maximum number of active doodad ProceduralMeshComponents. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 MaxActiveDoodads = 200;

    /** Maximum number of active WMO group ProceduralMeshComponents. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 MaxActiveWmoGroups = 100;

    /** Skip WMOs with more groups than this to avoid GPU overload. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 MaxWmoGroupsPerObject = 20;

private:
    TUniquePtr<FMpqManager> MpqManager;
    TUniquePtr<FWowAssetCache> AssetCache;
    TUniquePtr<FWdtData> WdtData;

    UPROPERTY()
    TMap<int64, TObjectPtr<AWowTerrainTile>> LoadedTiles;

    FIntPoint LastCameraTile = FIntPoint(-9999, -9999);

    /** Track spawned WMO unique IDs to avoid duplicates across tiles */
    TSet<uint32> SpawnedWmoIds;

    /** Track spawned doodad unique IDs to avoid duplicates across tiles */
    TSet<uint32> SpawnedDoodadIds;

    /** Current count of active doodad mesh components */
    int32 ActiveDoodadCount = 0;

    /** Current count of active WMO group mesh components */
    int32 ActiveWmoGroupCount = 0;

    void LoadTile(int32 TX, int32 TY);
    void LoadTileAsync(int32 TX, int32 TY);
    void FinalizeTileLoad(int32 TX, int32 TY, TSharedPtr<FAdtData> AdtData);
    void ProcessPendingLoads();
    void UnloadTile(int32 TX, int32 TY);
    void UpdateStreaming();
    void UpdateObjectStreaming();
    bool IsTileLoaded(int32 TX, int32 TY) const;
    bool IsTilePending(int32 TX, int32 TY) const;
    static int64 TileKey(int32 TX, int32 TY) { return ((int64)TX << 32) | (int64)(uint32)TY; }

    /** Tiles currently being loaded on background threads */
    TArray<FPendingTileLoad> PendingLoads;

    /** Track tile keys that are in-flight to avoid duplicate requests */
    TSet<int64> PendingTileKeys;
};
