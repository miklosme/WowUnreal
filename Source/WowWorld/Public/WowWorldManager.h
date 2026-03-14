#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "Formats/WdtTypes.h"
#include "WowWorldManager.generated.h"

class AWowTerrainTile;

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

private:
    TUniquePtr<FMpqManager> MpqManager;
    TUniquePtr<FWowAssetCache> AssetCache;
    TUniquePtr<FWdtData> WdtData;

    UPROPERTY()
    TMap<int64, TObjectPtr<AWowTerrainTile>> LoadedTiles;

    FIntPoint LastCameraTile = FIntPoint(-9999, -9999);

    /** Track spawned WMO unique IDs to avoid duplicates across tiles */
    TSet<uint32> SpawnedWmoIds;

    void LoadTile(int32 TX, int32 TY);
    void UnloadTile(int32 TX, int32 TY);
    void UpdateStreaming();
    bool IsTileLoaded(int32 TX, int32 TY) const;
    static int64 TileKey(int32 TX, int32 TY) { return ((int64)TX << 32) | (int64)(uint32)TY; }
};
