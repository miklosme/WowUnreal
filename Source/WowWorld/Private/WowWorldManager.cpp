#include "WowWorldManager.h"
#include "WowTerrainTile.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "Formats/AdtParser.h"
#include "Formats/WdtParser.h"
#include "Formats/WdtTypes.h"
#include "Coord/WowCoordinate.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowWorld, Log, All);

AWowWorldManager::AWowWorldManager() { PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = 0.5f; }

void AWowWorldManager::BeginPlay()
{
    Super::BeginPlay();
    MpqManager = MakeUnique<FMpqManager>();
    AssetCache = MakeUnique<FWowAssetCache>();
    FString DataPath;
    FParse::Value(FCommandLine::Get(), TEXT("-wowdata="), DataPath);
    if (DataPath.IsEmpty()) DataPath = TEXT("/Users/clancey/Downloads/World of Warcraft 3.3.5a/Data");
    if (!MpqManager->Initialize(DataPath))
    {
        UE_LOG(LogWowWorld, Error, TEXT("Failed to init MPQ from: %s"), *DataPath);
        return;
    }
    UE_LOG(LogWowWorld, Log, TEXT("World Manager ready: %s"), *MapName);

    // Load WDT for the map
    FString WdtPath = FString::Printf(TEXT("World\\Maps\\%s\\%s.wdt"), *MapName, *MapName);
    TArray<uint8> WdtRaw;
    if (MpqManager->ReadFile(WdtPath, WdtRaw))
    {
        WdtData = MakeUnique<FWdtData>(FWdtParser::Parse(WdtRaw));
        UE_LOG(LogWowWorld, Log, TEXT("Loaded WDT for %s (BigAlpha=%d)"), *MapName, WdtData->bUseBigAlpha ? 1 : 0);
    }
    else
    {
        UE_LOG(LogWowWorld, Warning, TEXT("Failed to load WDT: %s"), *WdtPath);
    }

    // Load an initial test tile
    LoadTile(DebugTileX, DebugTileY);
}

void AWowWorldManager::EndPlay(const EEndPlayReason::Type R)
{
    // Destroy all loaded tiles
    for (auto& Pair : LoadedTiles)
    {
        if (Pair.Value)
        {
            Pair.Value->Destroy();
        }
    }
    LoadedTiles.Empty();
    WdtData.Reset();

    if (AssetCache) AssetCache->Clear();
    if (MpqManager) MpqManager->Shutdown();
    Super::EndPlay(R);
}

void AWowWorldManager::Tick(float DT)
{
    Super::Tick(DT);

    if (!bStreamingEnabled || !MpqManager || !MpqManager->IsInitialized())
    {
        return;
    }

    UpdateStreaming();
}

void AWowWorldManager::LoadTile(int32 TX, int32 TY)
{
    if (IsTileLoaded(TX, TY))
    {
        return;
    }

    // Check WDT if available
    if (WdtData && WdtData->bIsValid)
    {
        if (TX < 0 || TX >= 64 || TY < 0 || TY >= 64 || !WdtData->TileExists[TX][TY])
        {
            UE_LOG(LogWowWorld, Verbose, TEXT("Tile %d,%d does not exist in WDT"), TX, TY);
            return;
        }
    }

    // Read ADT file from MPQ
    FString AdtPath = FString::Printf(TEXT("World\\Maps\\%s\\%s_%d_%d.adt"), *MapName, *MapName, TX, TY);
    TArray<uint8> AdtRaw;
    if (!MpqManager->ReadFile(AdtPath, AdtRaw))
    {
        UE_LOG(LogWowWorld, Warning, TEXT("Failed to read ADT: %s"), *AdtPath);
        return;
    }

    bool bBigAlpha = WdtData ? WdtData->bUseBigAlpha : false;
    FAdtData AdtData = FAdtParser::Parse(AdtRaw, bBigAlpha);
    if (!AdtData.bIsValid)
    {
        UE_LOG(LogWowWorld, Warning, TEXT("Failed to parse ADT: %s"), *AdtPath);
        return;
    }

    UE_LOG(LogWowWorld, Log, TEXT("Loading tile %d,%d (%s)"), TX, TY, *AdtPath);

    // Spawn terrain tile actor
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *FString::Printf(TEXT("TerrainTile_%d_%d"), TX, TY);
    AWowTerrainTile* Tile = GetWorld()->SpawnActor<AWowTerrainTile>(AWowTerrainTile::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (Tile)
    {
        Tile->BuildFromAdtData(AdtData, TX, TY, MpqManager.Get(), AssetCache.Get());
        LoadedTiles.Add(TileKey(TX, TY), Tile);
    }
}

void AWowWorldManager::UnloadTile(int32 TX, int32 TY)
{
    int64 Key = TileKey(TX, TY);
    TObjectPtr<AWowTerrainTile>* Found = LoadedTiles.Find(Key);
    if (Found && *Found)
    {
        (*Found)->Destroy();
        LoadedTiles.Remove(Key);
        UE_LOG(LogWowWorld, Log, TEXT("Unloaded tile %d,%d"), TX, TY);
    }
}

bool AWowWorldManager::IsTileLoaded(int32 TX, int32 TY) const
{
    return LoadedTiles.Contains(TileKey(TX, TY));
}

void AWowWorldManager::UpdateStreaming()
{
    // Get camera position
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    // Figure out which tile the camera is over
    FIntPoint CameraTile = FWowCoordinate::WorldToTile(CamLoc);

    // Only update if camera moved to a different tile
    if (CameraTile == LastCameraTile) return;
    LastCameraTile = CameraTile;

    UE_LOG(LogWowWorld, Verbose, TEXT("Camera tile: %d,%d"), CameraTile.X, CameraTile.Y);

    // Load tiles within radius
    for (int32 DX = -LoadRadius; DX <= LoadRadius; ++DX)
    {
        for (int32 DY = -LoadRadius; DY <= LoadRadius; ++DY)
        {
            int32 TX = CameraTile.X + DX;
            int32 TY = CameraTile.Y + DY;
            if (TX >= 0 && TX < 64 && TY >= 0 && TY < 64)
            {
                LoadTile(TX, TY);
            }
        }
    }

    // Unload tiles beyond unload radius
    TArray<int64> TilesToUnload;
    for (auto& Pair : LoadedTiles)
    {
        if (Pair.Value)
        {
            FIntPoint Coord = Pair.Value->GetTileCoord();
            int32 Dist = FMath::Max(FMath::Abs(Coord.X - CameraTile.X), FMath::Abs(Coord.Y - CameraTile.Y));
            if (Dist > UnloadRadius)
            {
                TilesToUnload.Add(Pair.Key);
            }
        }
    }
    for (int64 Key : TilesToUnload)
    {
        TObjectPtr<AWowTerrainTile>* Found = LoadedTiles.Find(Key);
        if (Found && *Found)
        {
            UE_LOG(LogWowWorld, Log, TEXT("Unloading tile %d,%d (too far)"), (*Found)->GetTileCoord().X, (*Found)->GetTileCoord().Y);
            (*Found)->Destroy();
        }
        LoadedTiles.Remove(Key);
    }
}
