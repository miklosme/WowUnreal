#include "WowWorldManager.h"
#include "WowTerrainTile.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "Formats/AdtParser.h"
#include "Formats/WdtParser.h"
#include "Formats/WdtTypes.h"
#include "Coord/WowCoordinate.h"
#include "Kismet/GameplayStatics.h"
#include "WowDoodadManager.h"
#include "WowWmoRenderer.h"

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

        // Log existing tiles for debugging
        int32 TileCount = 0;
        FString FirstTiles;
        for (int32 y = 0; y < 64; y++)
        {
            for (int32 x = 0; x < 64; x++)
            {
                if (WdtData->TileExists[x][y])
                {
                    TileCount++;
                    if (TileCount <= 10)
                    {
                        FirstTiles += FString::Printf(TEXT("(%d,%d) "), x, y);
                    }
                }
            }
        }
        UE_LOG(LogWowWorld, Log, TEXT("WDT has %d tiles. First: %s"), TileCount, *FirstTiles);

        // Check if debug tile exists
        if (DebugTileX >= 0 && DebugTileX < 64 && DebugTileY >= 0 && DebugTileY < 64)
        {
            UE_LOG(LogWowWorld, Log, TEXT("Debug tile %d,%d exists: %s"), DebugTileX, DebugTileY,
                WdtData->TileExists[DebugTileX][DebugTileY] ? TEXT("YES") : TEXT("NO"));
        }
    }
    else
    {
        UE_LOG(LogWowWorld, Warning, TEXT("Failed to load WDT: %s"), *WdtPath);
    }

    // Load a 3x3 grid of tiles around the debug tile
    for (int32 DX = -1; DX <= 1; ++DX)
    {
        for (int32 DY = -1; DY <= 1; ++DY)
        {
            LoadTile(DebugTileX + DX, DebugTileY + DY);
        }
    }

    // Enable streaming so more tiles load as camera moves
    bStreamingEnabled = true;

    // Teleport player above the loaded terrain
    // The tile actor is at TileToWorld position, vertices are local to that
    // First chunk (0,0) starts at local offset ~(-26667, -26667, height*100)
    // The height for Elwynn is ~236 WoW units = 23600 UE cm
    FVector TileCenter = FWowCoordinate::TileToWorld(DebugTileX, DebugTileY);
    // Start at a reasonable flying height (above terrain which is ~100-300 WoW units = 10000-30000 cm)
    TileCenter.Z = 40000.0f;  // 400m up, well above Elwynn terrain (~100-300 WoW units)

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC && PC->GetPawn())
    {
        PC->GetPawn()->SetActorLocation(TileCenter);
        // Look slightly down at the terrain (not straight down)
        PC->SetControlRotation(FRotator(-20.0f, 0.0f, 0.0f));
        UE_LOG(LogWowWorld, Log, TEXT("Teleported player to: %s"), *TileCenter.ToString());
    }
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
    UpdateObjectStreaming();
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
            UE_LOG(LogWowWorld, Warning, TEXT("Tile %d,%d does not exist in WDT"), TX, TY);
            return;
        }
    }
    else
    {
        UE_LOG(LogWowWorld, Warning, TEXT("No valid WDT data, attempting tile load anyway"));
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
        Tile->BuildFromAdtData(AdtData, TX, TY, MpqManager.Get(), AssetCache.Get(), &SpawnedWmoIds);
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
            AWowTerrainTile* Tile = *Found;
            UE_LOG(LogWowWorld, Log, TEXT("Unloading tile %d,%d (too far)"), Tile->GetTileCoord().X, Tile->GetTileCoord().Y);

            // Clean up spawned objects tracked by this tile
            for (auto& DoodadPair : Tile->SpawnedDoodads)
            {
                if (DoodadPair.Value)
                {
                    DoodadPair.Value->DestroyComponent();
                    --ActiveDoodadCount;
                }
                SpawnedDoodadIds.Remove(DoodadPair.Key);
            }
            for (auto& WmoPair : Tile->SpawnedWmos)
            {
                if (WmoPair.Value)
                {
                    WmoPair.Value->Destroy();
                    --ActiveWmoGroupCount; // approximate; exact tracking not critical here
                }
                SpawnedWmoIds.Remove(WmoPair.Key);
            }

            Tile->Destroy();
        }
        LoadedTiles.Remove(Key);
    }
}

void AWowWorldManager::UpdateObjectStreaming()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    const float DoodadRadiusSq = DoodadRadius * DoodadRadius;
    const float DoodadDespawnRadiusSq = (DoodadRadius * 1.2f) * (DoodadRadius * 1.2f);
    const float WmoRadiusSq = WmoRadius * WmoRadius;
    const float WmoDespawnRadiusSq = (WmoRadius * 1.2f) * (WmoRadius * 1.2f);

    // Iterate all loaded tiles
    for (auto& TilePair : LoadedTiles)
    {
        AWowTerrainTile* Tile = TilePair.Value;
        if (!Tile || !Tile->CachedMpq) continue;

        // --- DOODADS: despawn out-of-range ---
        {
            TArray<uint32> ToDespawn;
            for (auto& Pair : Tile->SpawnedDoodads)
            {
                if (!Pair.Value) { ToDespawn.Add(Pair.Key); continue; }
                float DistSq = FVector::DistSquared(CamLoc, Pair.Value->GetComponentLocation());
                if (DistSq > DoodadDespawnRadiusSq)
                {
                    ToDespawn.Add(Pair.Key);
                }
            }
            for (uint32 Id : ToDespawn)
            {
                TObjectPtr<UProceduralMeshComponent>* Comp = Tile->SpawnedDoodads.Find(Id);
                if (Comp && *Comp)
                {
                    (*Comp)->DestroyComponent();
                    --ActiveDoodadCount;
                }
                Tile->SpawnedDoodads.Remove(Id);
                SpawnedDoodadIds.Remove(Id);
            }
        }

        // --- DOODADS: spawn in-range ---
        for (const FAdtDoodadPlacement& Placement : Tile->DoodadPlacements)
        {
            if (ActiveDoodadCount >= MaxActiveDoodads) break;

            // Already spawned globally (could be on this tile or another)
            if (SpawnedDoodadIds.Contains(Placement.UniqueId)) continue;

            // Resolve path
            if (Placement.NameIndex < 0 || Placement.NameIndex >= Tile->DoodadPaths.Num()) continue;
            const FString& M2Path = Tile->DoodadPaths[Placement.NameIndex];
            if (M2Path.IsEmpty()) continue;

            // Distance check using noggit->UE position
            FVector UEPos = FWowCoordinate::NoggitToUE(
                Placement.Position.X, Placement.Position.Y, Placement.Position.Z);
            float DistSq = FVector::DistSquared(CamLoc, UEPos);
            if (DistSq > DoodadRadiusSq) continue;

            // Spawn it
            UProceduralMeshComponent* Comp = FWowDoodadManager::SpawnSingleDoodad(
                Tile, Placement, M2Path, Tile->CachedMpq, Tile->CachedCache);
            if (Comp)
            {
                Tile->SpawnedDoodads.Add(Placement.UniqueId, Comp);
                SpawnedDoodadIds.Add(Placement.UniqueId);
                ++ActiveDoodadCount;
            }
        }

        // --- WMOs: despawn out-of-range ---
        {
            TArray<uint32> ToDespawn;
            for (auto& Pair : Tile->SpawnedWmos)
            {
                if (!Pair.Value) { ToDespawn.Add(Pair.Key); continue; }
                float DistSq = FVector::DistSquared(CamLoc, Pair.Value->GetActorLocation());
                if (DistSq > WmoDespawnRadiusSq)
                {
                    ToDespawn.Add(Pair.Key);
                }
            }
            for (uint32 Id : ToDespawn)
            {
                TObjectPtr<AActor>* Act = Tile->SpawnedWmos.Find(Id);
                if (Act && *Act)
                {
                    (*Act)->Destroy();
                    --ActiveWmoGroupCount; // approximate
                }
                Tile->SpawnedWmos.Remove(Id);
                SpawnedWmoIds.Remove(Id);
            }
        }

        // --- WMOs: spawn in-range ---
        for (const FAdtWmoPlacement& Placement : Tile->WmoPlacements)
        {
            if (ActiveWmoGroupCount >= MaxActiveWmoGroups) break;

            // Already spawned globally
            if (SpawnedWmoIds.Contains(Placement.UniqueId)) continue;

            // Resolve path
            if (Placement.NameIndex < 0 || Placement.NameIndex >= Tile->WmoPaths.Num()) continue;
            const FString& WmoPath = Tile->WmoPaths[Placement.NameIndex];
            if (WmoPath.IsEmpty()) continue;

            // Distance check
            FVector UEPos = FWowCoordinate::NoggitToUE(
                Placement.Position.X, Placement.Position.Y, Placement.Position.Z);
            float DistSq = FVector::DistSquared(CamLoc, UEPos);
            if (DistSq > WmoRadiusSq) continue;

            // Check group count - skip huge WMOs (e.g. Stormwind)
            uint32 GroupCount = FWowWmoRenderer::GetWmoGroupCount(WmoPath, Tile->CachedMpq);
            if (GroupCount == 0) continue;
            if ((int32)GroupCount > MaxWmoGroupsPerObject)
            {
                // Mark as "spawned" so we don't re-check every tick
                SpawnedWmoIds.Add(Placement.UniqueId);
                UE_LOG(LogWowWorld, Log, TEXT("Skipping large WMO %s (%d groups > max %d)"),
                    *WmoPath, GroupCount, MaxWmoGroupsPerObject);
                continue;
            }

            // Check if adding this WMO would exceed the group limit
            if (ActiveWmoGroupCount + (int32)GroupCount > MaxActiveWmoGroups) continue;

            // Spawn it
            AActor* WmoActor = FWowWmoRenderer::SpawnWmo(
                GetWorld(), WmoPath, Placement, Tile->CachedMpq, Tile->CachedCache);
            if (WmoActor)
            {
                WmoActor->AttachToActor(Tile, FAttachmentTransformRules::KeepWorldTransform);
                Tile->SpawnedWmos.Add(Placement.UniqueId, WmoActor);
                SpawnedWmoIds.Add(Placement.UniqueId);
                ActiveWmoGroupCount += (int32)GroupCount;
            }
        }
    }
}
