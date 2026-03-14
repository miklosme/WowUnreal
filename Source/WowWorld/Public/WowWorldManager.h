#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "Formats/WdtTypes.h"
#include "Formats/WdlTypes.h"
#include "Formats/AdtTypes.h"
#include "Async/Future.h"
#include "WowWorldManager.generated.h"

class AWowTerrainTile;
class URuntimeVirtualTexture;
class URuntimeVirtualTextureComponent;

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

    /** Get the terrain RVT for mesh components to write to */
    URuntimeVirtualTexture* GetTerrainRVT() const { return TerrainRVT; }

    /** HUD info accessors */
    int32 GetLoadedTileCount() const { return LoadedTiles.Num(); }
    int32 GetPendingTileCount() const { return PendingTileKeys.Num(); }
    FIntPoint GetCameraTile() const { return LastCameraTile; }
    int32 GetActiveDoodadCount() const { return ActiveDoodadCount; }
    int32 GetActiveWmoGroupCount() const { return ActiveWmoGroupCount; }
    const FString& GetMapName() const { return MapName; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW")
    FString MapName = TEXT("Azeroth");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 LoadRadius = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 UnloadRadius = 4;

    /** Radius in tiles for WDL distant terrain (LOD 2) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 WdlRadius = 12;

    /** Distance beyond WdlRadius to unload WDL tiles */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 WdlUnloadRadius = 15;

    /** Radius in tiles for LOD 1 simplified terrain (between LoadRadius and WdlRadius) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 Lod1Radius = 5;

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
    TUniquePtr<FWdlData> WdlData;

    UPROPERTY()
    TMap<int64, TObjectPtr<AWowTerrainTile>> LoadedTiles;

    /** WDL distant terrain actors (LOD 2) */
    UPROPERTY()
    TMap<int64, TObjectPtr<AActor>> WdlTiles;

    /** LOD 1 simplified terrain tiles (mid-distance) */
    UPROPERTY()
    TMap<int64, TObjectPtr<AActor>> Lod1Tiles;

    /** Runtime Virtual Texture for terrain */
    UPROPERTY()
    TObjectPtr<URuntimeVirtualTexture> TerrainRVT;

    /** RVT volume component defining the virtual texture bounds */
    UPROPERTY()
    TObjectPtr<URuntimeVirtualTextureComponent> RVTVolumeComponent;

    FIntPoint LastCameraTile = FIntPoint(-9999, -9999);

    /** Track spawned WMO unique IDs to avoid duplicates across tiles */
    TSet<uint32> SpawnedWmoIds;

    /** Track spawned doodad unique IDs to avoid duplicates across tiles */
    TSet<uint32> SpawnedDoodadIds;

    /** Current count of active doodad mesh components */
    int32 ActiveDoodadCount = 0;

    /** Current count of active WMO group mesh components */
    int32 ActiveWmoGroupCount = 0;

    /** Optional command-line driven screenshot path and timing for automated verification runs. */
    FString AutoScreenshotPath;
    float AutoScreenshotDelaySeconds = -1.0f;
    float AutoQuitDelaySeconds = -1.0f;
    double AutoStartWallClockSeconds = 0.0;
    bool bAutoScreenshotRequested = false;
    bool bAutoQuitRequested = false;

    void LoadTile(int32 TX, int32 TY);
    void LoadTileAsync(int32 TX, int32 TY);
    void FinalizeTileLoad(int32 TX, int32 TY, TSharedPtr<FAdtData> AdtData);
    void ProcessPendingLoads();
    void UnloadTile(int32 TX, int32 TY);
    void UpdateStreaming();
    void UpdateObjectStreaming();
    void UpdateWdlStreaming(const FIntPoint& CameraTile);
    void UpdateLod1Streaming(const FIntPoint& CameraTile);
    void SpawnWdlTile(int32 TX, int32 TY);
    void SpawnLod1Tile(int32 TX, int32 TY);
    void SetupRuntimeVirtualTexture();
    void UpdateRVTBounds(const FIntPoint& CameraTile);
    bool IsTileLoaded(int32 TX, int32 TY) const;
    bool IsTilePending(int32 TX, int32 TY) const;
    static int64 TileKey(int32 TX, int32 TY) { return ((int64)TX << 32) | (int64)(uint32)TY; }

    /** Tiles currently being loaded on background threads */
    TArray<FPendingTileLoad> PendingLoads;

    /** Track tile keys that are in-flight to avoid duplicate requests */
    TSet<int64> PendingTileKeys;
};
