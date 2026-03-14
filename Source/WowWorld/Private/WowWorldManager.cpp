#include "WowWorldManager.h"
#include "WowTerrainTile.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "Formats/AdtParser.h"
#include "Formats/WdtParser.h"
#include "Formats/WdtTypes.h"
#include "Formats/WdlParser.h"
#include "Formats/WdlTypes.h"
#include "ProceduralMeshComponent.h"
#include "Coord/WowCoordinate.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WowDoodadManager.h"
#include "WowTerrainMaterial.h"
#include "WowWmoRenderer.h"
#include "Formats/Dbc/DbcStore.h"
#include "Async/Async.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Components/RuntimeVirtualTextureComponent.h"
#include "WowTerrainMeshBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowWorld, Log, All);

namespace
{
constexpr uint8 WdlTransition_North = 1 << 0;
constexpr uint8 WdlTransition_South = 1 << 1;
constexpr uint8 WdlTransition_West = 1 << 2;
constexpr uint8 WdlTransition_East = 1 << 3;
constexpr int32 WdlTransitionSamples = 16 * 8 + 1;
constexpr float WdlTransitionDepthOffset = 15.0f;

int32 OuterVertexIndex(int32 X, int32 Y)
{
    return Y * 17 + X;
}

int32 GetTileDistance(const FIntPoint& CameraTile, int32 TX, int32 TY)
{
    return FMath::Max(FMath::Abs(TX - CameraTile.X), FMath::Abs(TY - CameraTile.Y));
}

float GetChunkOuterHeight(const FAdtChunkData& Chunk, int32 OuterX, int32 OuterY)
{
    return Chunk.WorldZ + Chunk.Heights[OuterVertexIndex(OuterX, OuterY)];
}

bool IsNeighborInLod1Range(const FWdtData* WdtData, const FIntPoint& CameraTile, int32 LoadRadius, int32 Lod1Radius, int32 TX, int32 TY)
{
    if (!WdtData || TX < 0 || TX >= 64 || TY < 0 || TY >= 64 || !WdtData->TileExists[TX][TY])
    {
        return false;
    }

    const int32 Distance = GetTileDistance(CameraTile, TX, TY);
    return Distance > LoadRadius && Distance <= Lod1Radius;
}

uint8 DetermineWdlTransitionEdges(const FWdtData* WdtData, const FIntPoint& CameraTile, int32 LoadRadius, int32 Lod1Radius, int32 TX, int32 TY)
{
    uint8 EdgeMask = 0;
    if (IsNeighborInLod1Range(WdtData, CameraTile, LoadRadius, Lod1Radius, TX, TY - 1))
    {
        EdgeMask |= WdlTransition_North;
    }
    if (IsNeighborInLod1Range(WdtData, CameraTile, LoadRadius, Lod1Radius, TX, TY + 1))
    {
        EdgeMask |= WdlTransition_South;
    }
    if (IsNeighborInLod1Range(WdtData, CameraTile, LoadRadius, Lod1Radius, TX - 1, TY))
    {
        EdgeMask |= WdlTransition_West;
    }
    if (IsNeighborInLod1Range(WdtData, CameraTile, LoadRadius, Lod1Radius, TX + 1, TY))
    {
        EdgeMask |= WdlTransition_East;
    }
    return EdgeMask;
}

bool LoadTileAdtForTransition(FMpqManager* Mpq, const FString& MapName, bool bBigAlpha, int32 TX, int32 TY, FAdtData& OutAdtData)
{
    if (!Mpq)
    {
        return false;
    }

    const FString AdtPath = FString::Printf(TEXT("World\\Maps\\%s\\%s_%d_%d.adt"), *MapName, *MapName, TX, TY);
    TArray<uint8> AdtRaw;
    if (!Mpq->ReadFile(AdtPath, AdtRaw))
    {
        return false;
    }

    OutAdtData = FAdtParser::Parse(AdtRaw, bBigAlpha);
    return OutAdtData.bIsValid;
}

void BuildNorthEdgeHeights(const FAdtData& AdtData, TArray<float>& OutHeights)
{
    OutHeights.Reset();
    OutHeights.Reserve(WdlTransitionSamples);

    for (int32 ChunkX = 0; ChunkX < 16; ++ChunkX)
    {
        const FAdtChunkData& Chunk = AdtData.Chunks[ChunkX];
        for (int32 SampleX = 0; SampleX < 8; ++SampleX)
        {
            OutHeights.Add(GetChunkOuterHeight(Chunk, SampleX, 0));
        }
    }

    OutHeights.Add(GetChunkOuterHeight(AdtData.Chunks[15], 8, 0));
}

void BuildSouthEdgeHeights(const FAdtData& AdtData, TArray<float>& OutHeights)
{
    OutHeights.Reset();
    OutHeights.Reserve(WdlTransitionSamples);

    for (int32 ChunkX = 0; ChunkX < 16; ++ChunkX)
    {
        const FAdtChunkData& Chunk = AdtData.Chunks[15 * 16 + ChunkX];
        for (int32 SampleX = 0; SampleX < 8; ++SampleX)
        {
            OutHeights.Add(GetChunkOuterHeight(Chunk, SampleX, 8));
        }
    }

    OutHeights.Add(GetChunkOuterHeight(AdtData.Chunks[15 * 16 + 15], 8, 8));
}

void BuildWestEdgeHeights(const FAdtData& AdtData, TArray<float>& OutHeights)
{
    OutHeights.Reset();
    OutHeights.Reserve(WdlTransitionSamples);

    for (int32 ChunkY = 0; ChunkY < 16; ++ChunkY)
    {
        const FAdtChunkData& Chunk = AdtData.Chunks[ChunkY * 16];
        for (int32 SampleY = 0; SampleY < 8; ++SampleY)
        {
            OutHeights.Add(GetChunkOuterHeight(Chunk, 0, SampleY));
        }
    }

    OutHeights.Add(GetChunkOuterHeight(AdtData.Chunks[15 * 16], 0, 8));
}

void BuildEastEdgeHeights(const FAdtData& AdtData, TArray<float>& OutHeights)
{
    OutHeights.Reset();
    OutHeights.Reserve(WdlTransitionSamples);

    for (int32 ChunkY = 0; ChunkY < 16; ++ChunkY)
    {
        const FAdtChunkData& Chunk = AdtData.Chunks[ChunkY * 16 + 15];
        for (int32 SampleY = 0; SampleY < 8; ++SampleY)
        {
            OutHeights.Add(GetChunkOuterHeight(Chunk, 8, SampleY));
        }
    }

    OutHeights.Add(GetChunkOuterHeight(AdtData.Chunks[15 * 16 + 15], 8, 8));
}

float SampleWdlRowHeight(const FWdlTileData& TileData, int32 Row, int32 SampleIndex)
{
    if (SampleIndex >= WdlTransitionSamples - 1)
    {
        return static_cast<float>(TileData.Height17[Row][16]);
    }

    const int32 Segment = FMath::Clamp(SampleIndex / 8, 0, 15);
    const float Alpha = static_cast<float>(SampleIndex % 8) / 8.0f;
    return FMath::Lerp(
        static_cast<float>(TileData.Height17[Row][Segment]),
        static_cast<float>(TileData.Height17[Row][Segment + 1]),
        Alpha);
}

float SampleWdlColumnHeight(const FWdlTileData& TileData, int32 Column, int32 SampleIndex)
{
    if (SampleIndex >= WdlTransitionSamples - 1)
    {
        return static_cast<float>(TileData.Height17[16][Column]);
    }

    const int32 Segment = FMath::Clamp(SampleIndex / 8, 0, 15);
    const float Alpha = static_cast<float>(SampleIndex % 8) / 8.0f;
    return FMath::Lerp(
        static_cast<float>(TileData.Height17[Segment][Column]),
        static_cast<float>(TileData.Height17[Segment + 1][Column]),
        Alpha);
}

void ComputeSectionNormals(const TArray<FVector>& Vertices, const TArray<int32>& Indices, TArray<FVector>& OutNormals)
{
    OutNormals.Init(FVector::ZeroVector, Vertices.Num());

    for (int32 Index = 0; Index + 2 < Indices.Num(); Index += 3)
    {
        const FVector& A = Vertices[Indices[Index]];
        const FVector& B = Vertices[Indices[Index + 1]];
        const FVector& C = Vertices[Indices[Index + 2]];
        const FVector FaceNormal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();

        OutNormals[Indices[Index]] += FaceNormal;
        OutNormals[Indices[Index + 1]] += FaceNormal;
        OutNormals[Indices[Index + 2]] += FaceNormal;
    }

    for (FVector& Normal : OutNormals)
    {
        Normal = Normal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
    }
}

int32 AddWdlTransitionSection(
    UProceduralMeshComponent* Mesh,
    int32 SectionIndex,
    const FWdlTileData& TileData,
    int32 TX,
    int32 TY,
    uint8 Edge,
    const TArray<float>& OuterHeights,
    UMaterialInterface* Material)
{
    if (!Mesh || OuterHeights.Num() != WdlTransitionSamples)
    {
        return SectionIndex;
    }

    TArray<FVector> Vertices;
    TArray<int32> Indices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    Vertices.Reserve(WdlTransitionSamples * 2);
    UVs.Reserve(WdlTransitionSamples * 2);
    Indices.Reserve((WdlTransitionSamples - 1) * 6);

    const float TileNgX = TX * FWowCoordinate::TILE_SIZE;
    const float TileNgZ = TY * FWowCoordinate::TILE_SIZE;
    const float FineStep = FWowCoordinate::UNIT_SIZE;
    const float CoarseStep = FWowCoordinate::CHUNK_SIZE;

    for (int32 SampleIndex = 0; SampleIndex < WdlTransitionSamples; ++SampleIndex)
    {
        const float Offset = SampleIndex * FineStep;

        float OuterX = TileNgX;
        float OuterZ = TileNgZ;
        float InnerX = TileNgX;
        float InnerZ = TileNgZ;
        float InnerHeight = 0.0f;

        switch (Edge)
        {
        case WdlTransition_North:
            OuterX += Offset;
            InnerX += Offset;
            InnerZ += CoarseStep;
            InnerHeight = SampleWdlRowHeight(TileData, 1, SampleIndex);
            break;
        case WdlTransition_South:
            OuterX += Offset;
            OuterZ += FWowCoordinate::TILE_SIZE;
            InnerX += Offset;
            InnerZ += FWowCoordinate::TILE_SIZE - CoarseStep;
            InnerHeight = SampleWdlRowHeight(TileData, 15, SampleIndex);
            break;
        case WdlTransition_West:
            OuterZ += Offset;
            InnerX += CoarseStep;
            InnerZ += Offset;
            InnerHeight = SampleWdlColumnHeight(TileData, 1, SampleIndex);
            break;
        case WdlTransition_East:
            OuterX += FWowCoordinate::TILE_SIZE;
            OuterZ += Offset;
            InnerX += FWowCoordinate::TILE_SIZE - CoarseStep;
            InnerZ += Offset;
            InnerHeight = SampleWdlColumnHeight(TileData, 15, SampleIndex);
            break;
        default:
            return SectionIndex;
        }

        FVector OuterVertex = FWowCoordinate::AdtToUE(OuterX, OuterHeights[SampleIndex], OuterZ);
        FVector InnerVertex = FWowCoordinate::AdtToUE(InnerX, InnerHeight, InnerZ);
        OuterVertex.Z -= WdlTransitionDepthOffset;
        InnerVertex.Z -= WdlTransitionDepthOffset;

        Vertices.Add(OuterVertex);
        UVs.Add(FVector2D(static_cast<float>(SampleIndex) / (WdlTransitionSamples - 1), 0.0f));
    }

    for (int32 SampleIndex = 0; SampleIndex < WdlTransitionSamples; ++SampleIndex)
    {
        const float Offset = SampleIndex * FineStep;

        float InnerX = TileNgX;
        float InnerZ = TileNgZ;
        float InnerHeight = 0.0f;

        switch (Edge)
        {
        case WdlTransition_North:
            InnerX += Offset;
            InnerZ += CoarseStep;
            InnerHeight = SampleWdlRowHeight(TileData, 1, SampleIndex);
            break;
        case WdlTransition_South:
            InnerX += Offset;
            InnerZ += FWowCoordinate::TILE_SIZE - CoarseStep;
            InnerHeight = SampleWdlRowHeight(TileData, 15, SampleIndex);
            break;
        case WdlTransition_West:
            InnerX += CoarseStep;
            InnerZ += Offset;
            InnerHeight = SampleWdlColumnHeight(TileData, 1, SampleIndex);
            break;
        case WdlTransition_East:
            InnerX += FWowCoordinate::TILE_SIZE - CoarseStep;
            InnerZ += Offset;
            InnerHeight = SampleWdlColumnHeight(TileData, 15, SampleIndex);
            break;
        default:
            return SectionIndex;
        }

        FVector InnerVertex = FWowCoordinate::AdtToUE(InnerX, InnerHeight, InnerZ);
        InnerVertex.Z -= WdlTransitionDepthOffset;
        Vertices.Add(InnerVertex);
        UVs.Add(FVector2D(static_cast<float>(SampleIndex) / (WdlTransitionSamples - 1), 1.0f));
    }

    for (int32 SampleIndex = 0; SampleIndex < WdlTransitionSamples - 1; ++SampleIndex)
    {
        const int32 TopLeft = SampleIndex;
        const int32 TopRight = SampleIndex + 1;
        const int32 BottomLeft = WdlTransitionSamples + SampleIndex;
        const int32 BottomRight = BottomLeft + 1;

        Indices.Add(TopLeft);
        Indices.Add(BottomLeft);
        Indices.Add(TopRight);

        Indices.Add(TopRight);
        Indices.Add(BottomLeft);
        Indices.Add(BottomRight);
    }

    ComputeSectionNormals(Vertices, Indices, Normals);
    Mesh->CreateMeshSection_LinearColor(SectionIndex, Vertices, Indices, Normals, UVs, Colors, Tangents, false);
    if (Material)
    {
        Mesh->SetMaterial(SectionIndex, Material);
    }

    return SectionIndex + 1;
}
}

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

    // Load DBC tables
    FDbcStore::Get().LoadAll(*MpqManager);

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

    // Load WDL for distant terrain
    FString WdlPath = FString::Printf(TEXT("World\\Maps\\%s\\%s.wdl"), *MapName, *MapName);
    TArray<uint8> WdlRaw;
    if (MpqManager->ReadFile(WdlPath, WdlRaw))
    {
        WdlData = MakeUnique<FWdlData>(FWdlParser::Parse(WdlRaw));
        UE_LOG(LogWowWorld, Log, TEXT("Loaded WDL for %s (valid=%d)"), *MapName, WdlData->bIsValid ? 1 : 0);
    }
    else
    {
        UE_LOG(LogWowWorld, Warning, TEXT("No WDL file found: %s"), *WdlPath);
    }

    // Set up Runtime Virtual Texture for terrain
    SetupRuntimeVirtualTexture();

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
    // Wait for any pending async loads to complete before cleanup
    for (FPendingTileLoad& Pending : PendingLoads)
    {
        if (Pending.Future.IsValid())
        {
            Pending.Future.Wait();
        }
    }
    PendingLoads.Empty();
    PendingTileKeys.Empty();

    // Destroy all loaded tiles
    for (auto& Pair : LoadedTiles)
    {
        if (Pair.Value)
        {
            Pair.Value->Destroy();
        }
    }
    LoadedTiles.Empty();

    // Destroy WDL tiles
    for (auto& Pair : WdlTiles)
    {
        if (Pair.Value) Pair.Value->Destroy();
    }
    WdlTiles.Empty();

    // Destroy LOD 1 tiles
    for (auto& Pair : Lod1Tiles)
    {
        if (Pair.Value) Pair.Value->Destroy();
    }
    Lod1Tiles.Empty();

    WdtData.Reset();
    WdlData.Reset();

    if (AssetCache) AssetCache->Clear();
    if (MpqManager) MpqManager->Shutdown();
    Super::EndPlay(R);
}

void AWowWorldManager::Tick(float DT)
{
    Super::Tick(DT);

    // Always process completed async loads, even before streaming is enabled
    ProcessPendingLoads();

    if (!bStreamingEnabled || !MpqManager || !MpqManager->IsInitialized())
    {
        return;
    }

    UpdateStreaming();
    UpdateObjectStreaming();

    // Periodically purge stale cache entries and check budget
    static int32 CachePurgeCounter = 0;
    if (AssetCache && (++CachePurgeCounter % 20 == 0)) // every ~10 seconds at 0.5s tick
    {
        AssetCache->PurgeStaleEntries();
        if (AssetCache->IsOverBudget())
        {
            FWowCacheStats Stats = AssetCache->GetStats();
            UE_LOG(LogWowWorld, Warning, TEXT("Asset cache over budget: %lld MB (budget: %lld MB, %d textures, %d meshes)"),
                Stats.TotalEstimatedMemory() / (1024 * 1024),
                AssetCache->MemoryBudget / (1024 * 1024),
                Stats.TextureCount, Stats.MeshCount);
        }
    }
}

void AWowWorldManager::LoadTile(int32 TX, int32 TY)
{
    if (IsTileLoaded(TX, TY) || IsTilePending(TX, TY))
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
        if (TerrainRVT) Tile->ApplyRuntimeVirtualTexture(TerrainRVT);
        LoadedTiles.Add(TileKey(TX, TY), Tile);
    }
}

void AWowWorldManager::LoadTileAsync(int32 TX, int32 TY)
{
    if (IsTileLoaded(TX, TY) || IsTilePending(TX, TY))
    {
        return;
    }

    // Check WDT if available
    if (WdtData && WdtData->bIsValid)
    {
        if (TX < 0 || TX >= 64 || TY < 0 || TY >= 64 || !WdtData->TileExists[TX][TY])
        {
            return;
        }
    }

    int64 Key = TileKey(TX, TY);
    PendingTileKeys.Add(Key);

    UE_LOG(LogWowWorld, Log, TEXT("Async loading tile %d,%d"), TX, TY);

    // Capture what we need for the background thread
    FMpqManager* Mpq = MpqManager.Get();
    FString Map = MapName;
    bool bBigAlpha = WdtData ? WdtData->bUseBigAlpha : false;

    FPendingTileLoad Pending;
    Pending.TX = TX;
    Pending.TY = TY;
    Pending.Future = Async(EAsyncExecution::ThreadPool, [Mpq, Map, TX, TY, bBigAlpha]() -> TSharedPtr<FAdtData>
    {
        FString AdtPath = FString::Printf(TEXT("World\\Maps\\%s\\%s_%d_%d.adt"), *Map, *Map, TX, TY);
        TArray<uint8> AdtRaw;
        if (!Mpq->ReadFile(AdtPath, AdtRaw))
        {
            UE_LOG(LogWowWorld, Warning, TEXT("Async: Failed to read ADT: %s"), *AdtPath);
            return nullptr;
        }

        TSharedPtr<FAdtData> Result = MakeShared<FAdtData>(FAdtParser::Parse(AdtRaw, bBigAlpha));
        if (!Result->bIsValid)
        {
            UE_LOG(LogWowWorld, Warning, TEXT("Async: Failed to parse ADT: %s"), *AdtPath);
            return nullptr;
        }

        return Result;
    });

    PendingLoads.Add(MoveTemp(Pending));
}

void AWowWorldManager::FinalizeTileLoad(int32 TX, int32 TY, TSharedPtr<FAdtData> AdtData)
{
    if (!AdtData || IsTileLoaded(TX, TY))
    {
        return;
    }

    UE_LOG(LogWowWorld, Log, TEXT("Finalizing async tile %d,%d on game thread"), TX, TY);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *FString::Printf(TEXT("TerrainTile_%d_%d"), TX, TY);
    AWowTerrainTile* Tile = GetWorld()->SpawnActor<AWowTerrainTile>(AWowTerrainTile::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (Tile)
    {
        Tile->BuildFromAdtData(*AdtData, TX, TY, MpqManager.Get(), AssetCache.Get(), &SpawnedWmoIds);
        if (TerrainRVT) Tile->ApplyRuntimeVirtualTexture(TerrainRVT);
        LoadedTiles.Add(TileKey(TX, TY), Tile);
    }
}

void AWowWorldManager::ProcessPendingLoads()
{
    for (int32 i = PendingLoads.Num() - 1; i >= 0; --i)
    {
        FPendingTileLoad& Pending = PendingLoads[i];
        if (Pending.Future.IsReady())
        {
            TSharedPtr<FAdtData> Result = Pending.Future.Get();
            int64 Key = TileKey(Pending.TX, Pending.TY);
            PendingTileKeys.Remove(Key);

            if (Result)
            {
                FinalizeTileLoad(Pending.TX, Pending.TY, Result);
            }

            PendingLoads.RemoveAt(i);
        }
    }
}

bool AWowWorldManager::IsTilePending(int32 TX, int32 TY) const
{
    return PendingTileKeys.Contains(TileKey(TX, TY));
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

    // Update WDL distant terrain
    UpdateWdlStreaming(CameraTile);

    // Update LOD 1 terrain
    UpdateLod1Streaming(CameraTile);

    // Keep RVT volume centered on camera
    UpdateRVTBounds(CameraTile);

    UE_LOG(LogWowWorld, Verbose, TEXT("Camera tile: %d,%d"), CameraTile.X, CameraTile.Y);

    // Load tiles within radius (async to avoid main-thread stalls)
    for (int32 DX = -LoadRadius; DX <= LoadRadius; ++DX)
    {
        for (int32 DY = -LoadRadius; DY <= LoadRadius; ++DY)
        {
            int32 TX = CameraTile.X + DX;
            int32 TY = CameraTile.Y + DY;
            if (TX >= 0 && TX < 64 && TY >= 0 && TY < 64)
            {
                LoadTileAsync(TX, TY);
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
            // HISMC doodads are destroyed automatically with the tile actor
            if (!Tile->bUsesInstancedDoodads)
            {
                for (auto& DoodadPair : Tile->SpawnedDoodads)
                {
                    if (DoodadPair.Value)
                    {
                        DoodadPair.Value->DestroyComponent();
                        --ActiveDoodadCount;
                    }
                    SpawnedDoodadIds.Remove(DoodadPair.Key);
                }
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

void AWowWorldManager::UpdateWdlStreaming(const FIntPoint& CameraTile)
{
    if (!WdlData || !WdlData->bIsValid) return;

    // Load WDL tiles in range (beyond LOD 0 radius, up to WdlRadius)
    for (int32 DX = -WdlRadius; DX <= WdlRadius; ++DX)
    {
        for (int32 DY = -WdlRadius; DY <= WdlRadius; ++DY)
        {
            int32 TX = CameraTile.X + DX;
            int32 TY = CameraTile.Y + DY;
            if (TX < 0 || TX >= 64 || TY < 0 || TY >= 64) continue;

            // Skip tiles that have full-detail ADT loaded, pending, or LOD 1
            if (IsTileLoaded(TX, TY) || IsTilePending(TX, TY)) continue;
            if (Lod1Tiles.Contains(TileKey(TX, TY))) continue;

            // Skip tiles already loaded as WDL
            int64 Key = TileKey(TX, TY);
            if (WdlTiles.Contains(Key)) continue;

            // Must have WDL data for this tile
            if (!WdlData->HasTile(TX, TY)) continue;

            SpawnWdlTile(TX, TY);
        }
    }

    // Unload WDL tiles beyond unload radius or that now have full ADT loaded
    TArray<int64> ToUnload;
    for (auto& Pair : WdlTiles)
    {
        int32 TX = (int32)(Pair.Key >> 32);
        int32 TY = (int32)(Pair.Key & 0xFFFFFFFF);
        int32 Dist = FMath::Max(FMath::Abs(TX - CameraTile.X), FMath::Abs(TY - CameraTile.Y));

        // Unload if too far or if full-detail tile is now loaded
        if (Dist > WdlUnloadRadius || IsTileLoaded(TX, TY))
        {
            ToUnload.Add(Pair.Key);
        }
    }
    for (int64 Key : ToUnload)
    {
        TObjectPtr<AActor>* Found = WdlTiles.Find(Key);
        if (Found && *Found) (*Found)->Destroy();
        WdlTiles.Remove(Key);
    }
}

void AWowWorldManager::SpawnWdlTile(int32 TX, int32 TY)
{
    if (!WdlData || !WdlData->HasTile(TX, TY)) return;

    const FWdlTileData& TileData = *WdlData->Tiles[TY][TX];

    // Build a 17x17 mesh from WDL heights
    TArray<FVector> Vertices;
    TArray<int32> Indices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;

    Vertices.Reserve(17 * 17);
    UVs.Reserve(17 * 17);

    // WDL height_17 covers the full tile in a 17x17 grid
    // Each step = TILE_SIZE / 16
    float StepSize = FWowCoordinate::TILE_SIZE / 16.0f;

    // Tile origin in ADT space: NgX = TX * TILE_SIZE, NgZ = TY * TILE_SIZE
    float TileNgX = TX * FWowCoordinate::TILE_SIZE;
    float TileNgZ = TY * FWowCoordinate::TILE_SIZE;

    for (int32 Row = 0; Row < 17; Row++)
    {
        for (int32 Col = 0; Col < 17; Col++)
        {
            float NgX = TileNgX + Col * StepSize;
            float NgZ = TileNgZ + Row * StepSize;
            float NgY = (float)TileData.Height17[Row][Col];

            FVector UEPos = FWowCoordinate::AdtToUE(NgX, NgY, NgZ);

            Vertices.Add(UEPos);
            UVs.Add(FVector2D((float)Col / 16.0f, (float)Row / 16.0f));
        }
    }

    // Build triangle indices (two triangles per quad)
    Indices.Reserve(16 * 16 * 6);
    for (int32 Row = 0; Row < 16; Row++)
    {
        for (int32 Col = 0; Col < 16; Col++)
        {
            // Skip holes from MAHO data
            if (TileData.IsHole(Col, Row))
                continue;

            int32 TL = Row * 17 + Col;
            int32 TR = TL + 1;
            int32 BL = TL + 17;
            int32 BR = BL + 1;

            Indices.Add(TL);
            Indices.Add(BL);
            Indices.Add(TR);

            Indices.Add(TR);
            Indices.Add(BL);
            Indices.Add(BR);
        }
    }

    // Compute flat normals per vertex (simple average of adjacent face normals)
    Normals.SetNumZeroed(Vertices.Num());
    for (int32 i = 0; i < Indices.Num(); i += 3)
    {
        FVector& V0 = Vertices[Indices[i]];
        FVector& V1 = Vertices[Indices[i + 1]];
        FVector& V2 = Vertices[Indices[i + 2]];
        FVector FaceNormal = FVector::CrossProduct(V1 - V0, V2 - V0).GetSafeNormal();
        Normals[Indices[i]] += FaceNormal;
        Normals[Indices[i + 1]] += FaceNormal;
        Normals[Indices[i + 2]] += FaceNormal;
    }
    for (FVector& N : Normals) N = N.GetSafeNormal();

    // Spawn actor with ProceduralMeshComponent
    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *FString::Printf(TEXT("WdlTile_%d_%d"), TX, TY);
    AActor* WdlActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (!WdlActor) return;

    UProceduralMeshComponent* Mesh = NewObject<UProceduralMeshComponent>(WdlActor, TEXT("WdlMesh"));
    Mesh->RegisterComponent();
    WdlActor->SetRootComponent(Mesh);

    TArray<FVector2D> EmptyUV2;
    TArray<FLinearColor> EmptyColors;
    TArray<FProcMeshTangent> EmptyTangents;

    Mesh->CreateMeshSection_LinearColor(0, Vertices, Indices, Normals, UVs, EmptyColors, EmptyTangents, false);

    // Simple green-brown material for distant terrain
    UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(
        UMaterial::GetDefaultMaterial(MD_Surface), WdlActor);
    if (Mat)
    {
        Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.3f, 0.4f, 0.2f, 1.0f));
        Mesh->SetMaterial(0, Mat);
    }

    // Disable collision and shadow for distant terrain
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);

    const uint8 TransitionEdges = DetermineWdlTransitionEdges(WdtData.Get(), LastCameraTile, LoadRadius, Lod1Radius, TX, TY);
    if (TransitionEdges != 0)
    {
        FAdtData TransitionAdtData;
        if (LoadTileAdtForTransition(MpqManager.Get(), MapName, WdtData ? WdtData->bUseBigAlpha : false, TX, TY, TransitionAdtData))
        {
            int32 TransitionSectionIndex = 1;
            int32 TransitionSectionCount = 0;
            TArray<float> EdgeHeights;
            UMaterialInterface* TransitionMaterial = Mat ? static_cast<UMaterialInterface*>(Mat) : static_cast<UMaterialInterface*>(UMaterial::GetDefaultMaterial(MD_Surface));

            if ((TransitionEdges & WdlTransition_North) != 0)
            {
                BuildNorthEdgeHeights(TransitionAdtData, EdgeHeights);
                TransitionSectionIndex = AddWdlTransitionSection(Mesh, TransitionSectionIndex, TileData, TX, TY, WdlTransition_North, EdgeHeights, TransitionMaterial);
                TransitionSectionCount++;
            }
            if ((TransitionEdges & WdlTransition_South) != 0)
            {
                BuildSouthEdgeHeights(TransitionAdtData, EdgeHeights);
                TransitionSectionIndex = AddWdlTransitionSection(Mesh, TransitionSectionIndex, TileData, TX, TY, WdlTransition_South, EdgeHeights, TransitionMaterial);
                TransitionSectionCount++;
            }
            if ((TransitionEdges & WdlTransition_West) != 0)
            {
                BuildWestEdgeHeights(TransitionAdtData, EdgeHeights);
                TransitionSectionIndex = AddWdlTransitionSection(Mesh, TransitionSectionIndex, TileData, TX, TY, WdlTransition_West, EdgeHeights, TransitionMaterial);
                TransitionSectionCount++;
            }
            if ((TransitionEdges & WdlTransition_East) != 0)
            {
                BuildEastEdgeHeights(TransitionAdtData, EdgeHeights);
                TransitionSectionIndex = AddWdlTransitionSection(Mesh, TransitionSectionIndex, TileData, TX, TY, WdlTransition_East, EdgeHeights, TransitionMaterial);
                TransitionSectionCount++;
            }

            UE_LOG(LogWowWorld, Log, TEXT("Spawned WDL tile %d,%d with %d transition strip sections"), TX, TY, TransitionSectionCount);
        }
        else
        {
            UE_LOG(LogWowWorld, Warning, TEXT("Failed to load ADT border data for WDL transition tile %d,%d"), TX, TY);
        }
    }

    WdlTiles.Add(TileKey(TX, TY), WdlActor);

    UE_LOG(LogWowWorld, Verbose, TEXT("Spawned WDL tile %d,%d (%d verts)"), TX, TY, Vertices.Num());
}

void AWowWorldManager::UpdateLod1Streaming(const FIntPoint& CameraTile)
{
    if (!WdtData || !WdtData->bIsValid || !MpqManager) return;

    // Load LOD 1 tiles in mid-range (beyond full-detail, within Lod1Radius)
    for (int32 DX = -Lod1Radius; DX <= Lod1Radius; ++DX)
    {
        for (int32 DY = -Lod1Radius; DY <= Lod1Radius; ++DY)
        {
            int32 Dist = FMath::Max(FMath::Abs(DX), FMath::Abs(DY));
            // Only LOD 1 range: beyond LoadRadius, within Lod1Radius
            if (Dist <= LoadRadius) continue;

            int32 TX = CameraTile.X + DX;
            int32 TY = CameraTile.Y + DY;
            if (TX < 0 || TX >= 64 || TY < 0 || TY >= 64) continue;
            if (!WdtData->TileExists[TX][TY]) continue;

            // Skip if full ADT loaded or pending
            if (IsTileLoaded(TX, TY) || IsTilePending(TX, TY)) continue;

            int64 Key = TileKey(TX, TY);
            if (Lod1Tiles.Contains(Key)) continue;

            SpawnLod1Tile(TX, TY);
        }
    }

    // Unload LOD 1 tiles that are too far or superseded by full ADT
    TArray<int64> ToUnload;
    for (auto& Pair : Lod1Tiles)
    {
        int32 TX = (int32)(Pair.Key >> 32);
        int32 TY = (int32)(Pair.Key & 0xFFFFFFFF);
        int32 Dist = FMath::Max(FMath::Abs(TX - CameraTile.X), FMath::Abs(TY - CameraTile.Y));

        if (Dist > Lod1Radius + 1 || IsTileLoaded(TX, TY))
        {
            ToUnload.Add(Pair.Key);
        }
    }
    for (int64 Key : ToUnload)
    {
        TObjectPtr<AActor>* Found = Lod1Tiles.Find(Key);
        if (Found && *Found) (*Found)->Destroy();
        Lod1Tiles.Remove(Key);
    }
}

void AWowWorldManager::SpawnLod1Tile(int32 TX, int32 TY)
{
    FString AdtPath = FString::Printf(TEXT("World\\Maps\\%s\\%s_%d_%d.adt"), *MapName, *MapName, TX, TY);
    TArray<uint8> AdtRaw;
    if (!MpqManager->ReadFile(AdtPath, AdtRaw))
    {
        return;
    }

    bool bBigAlpha = WdtData ? WdtData->bUseBigAlpha : false;
    FAdtData AdtData = FAdtParser::Parse(AdtRaw, bBigAlpha);
    if (!AdtData.bIsValid) return;

    FVector TileCenter = FWowCoordinate::TileToWorld(TX, TY);

    FActorSpawnParameters SpawnParams;
    SpawnParams.Name = *FString::Printf(TEXT("Lod1Tile_%d_%d"), TX, TY);
    AActor* Lod1Actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), TileCenter, FRotator::ZeroRotator, SpawnParams);
    if (!Lod1Actor) return;

    USceneComponent* Root = NewObject<USceneComponent>(Lod1Actor, TEXT("Root"));
    Root->RegisterComponent();
    Lod1Actor->SetRootComponent(Root);

    UProceduralMeshComponent* MeshComp = NewObject<UProceduralMeshComponent>(Lod1Actor, TEXT("Lod1Mesh"));
    MeshComp->SetupAttachment(Root);
    MeshComp->RegisterComponent();
    MeshComp->SetCastShadow(false);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    int32 SectionIndex = 0;
    int32 TexturedSections = 0;
    UMaterialInterface* DefaultLod1Material = FWowTerrainMaterial::GetBaseMaterial();
    if (!DefaultLod1Material)
    {
        DefaultLod1Material = UMaterial::GetDefaultMaterial(MD_Surface);
    }

    for (int32 i = 0; i < 256; ++i)
    {
        const FAdtChunkData& ChunkData = AdtData.Chunks[i];
        FTerrainChunkMeshData MeshData = FTerrainMeshBuilder::BuildChunkMeshLOD1(ChunkData, TX, TY);
        if (MeshData.Vertices.Num() == 0 || MeshData.Indices.Num() == 0) continue;

        TArray<FLinearColor> Colors;
        TArray<FProcMeshTangent> Tangents;
        Colors.Reserve(MeshData.VertexColors.Num());
        Tangents.Reserve(MeshData.Normals.Num());

        for (const FColor& Color : MeshData.VertexColors)
        {
            Colors.Add(FLinearColor(Color));
        }

        for (const FVector& Normal : MeshData.Normals)
        {
            FVector Tangent = FVector::CrossProduct(Normal, FVector(0, 1, 0));
            if (Tangent.SizeSquared() < 0.001f)
            {
                Tangent = FVector::CrossProduct(Normal, FVector(1, 0, 0));
            }
            Tangent.Normalize();
            Tangents.Add(FProcMeshTangent(Tangent, false));
        }

        MeshComp->CreateMeshSection_LinearColor(
            SectionIndex, MeshData.Vertices, MeshData.Indices,
            MeshData.Normals, MeshData.UVs, Colors, Tangents, false);

        UMaterialInstanceDynamic* ChunkMaterial = FWowTerrainMaterial::CreateChunkMaterial(
            ChunkData, AdtData, MpqManager.Get(), AssetCache.Get(), Lod1Actor);

        if (ChunkMaterial)
        {
            MeshComp->SetMaterial(SectionIndex, ChunkMaterial);
            TexturedSections++;
        }
        else if (DefaultLod1Material)
        {
            MeshComp->SetMaterial(SectionIndex, DefaultLod1Material);
        }

        SectionIndex++;
    }

    Lod1Tiles.Add(TileKey(TX, TY), Lod1Actor);
    UE_LOG(LogWowWorld, Log, TEXT("Spawned LOD 1 tile %d,%d (%d sections, %d textured)"), TX, TY, SectionIndex, TexturedSections);
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

    // Debug: log once per second
    static int32 DebugCounter = 0;
    bool bDebugLog = (DebugCounter++ % 2 == 0); // every other tick (1 second at 0.5s tick)
    if (bDebugLog)
    {
        int32 TotalDoodads = 0, TotalWmos = 0;
        for (auto& TP : LoadedTiles)
        {
            if (TP.Value) { TotalDoodads += TP.Value->DoodadPlacements.Num(); TotalWmos += TP.Value->WmoPlacements.Num(); }
        }
        UE_LOG(LogWowWorld, Log, TEXT("ObjectStreaming: cam=%s, %d tiles, %d total doodads, %d total wmos, active: %d doodads, %d wmo groups"),
            *CamLoc.ToString(), LoadedTiles.Num(), TotalDoodads, TotalWmos, ActiveDoodadCount, ActiveWmoGroupCount);
    }

    // Iterate all loaded tiles
    for (auto& TilePair : LoadedTiles)
    {
        AWowTerrainTile* Tile = TilePair.Value;
        if (!Tile || !Tile->CachedMpq) continue;

        // --- DOODADS: Skip per-instance streaming if tile uses HISMC instancing ---
        // HISMC handles culling automatically via built-in LOD and cull distances
        if (!Tile->bUsesInstancedDoodads)
        {
            // Legacy per-instance doodad streaming (fallback)
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

                // Distance check: convert MDDF to ADT space then UE (matches terrain)
                FVector UEPos = FWowCoordinate::AdtToUE(
                    Placement.Position.X,
                    Placement.Position.Y,
                    Placement.Position.Z);
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

            // Distance check: convert MODF to ADT space then UE (matches terrain)
            FVector UEPos = FWowCoordinate::AdtToUE(
                Placement.Position.X,
                Placement.Position.Y,
                Placement.Position.Z);
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

void AWowWorldManager::SetupRuntimeVirtualTexture()
{
    // Create the Runtime Virtual Texture object
    // Uses defaults: BaseColor_Normal_Specular, TileCount=8 (256), TileSize=2 (256px)
    TerrainRVT = NewObject<URuntimeVirtualTexture>(this, TEXT("TerrainRVT"));
    if (!TerrainRVT)
    {
        UE_LOG(LogWowWorld, Warning, TEXT("Failed to create Runtime Virtual Texture"));
        return;
    }

    // Create the RVT volume component that defines world bounds
    RVTVolumeComponent = NewObject<URuntimeVirtualTextureComponent>(this, TEXT("RVTVolume"));
    if (RVTVolumeComponent)
    {
        RVTVolumeComponent->SetupAttachment(GetRootComponent());
        RVTVolumeComponent->RegisterComponent();
        RVTVolumeComponent->SetVirtualTexture(TerrainRVT);

        // Set initial bounds covering loaded terrain area
        // Scale defines the volume extents - RVT bounds come from CalcBounds using transform
        float TileSizeUE = FWowCoordinate::TILE_SIZE * 100.0f; // ADT units to UE cm
        float CoverageHalfSize = (LoadRadius + 1) * TileSizeUE;
        FVector TileCenter = FWowCoordinate::TileToWorld(DebugTileX, DebugTileY);

        RVTVolumeComponent->SetWorldLocation(TileCenter);
        RVTVolumeComponent->SetWorldScale3D(FVector(CoverageHalfSize * 2.0f, CoverageHalfSize * 2.0f, 100000.0f));

        UE_LOG(LogWowWorld, Log, TEXT("RVT volume at %s, coverage %.0f cm"), *TileCenter.ToString(), CoverageHalfSize);
    }

    UE_LOG(LogWowWorld, Log, TEXT("Runtime Virtual Texture created for terrain"));
}

void AWowWorldManager::UpdateRVTBounds(const FIntPoint& CameraTile)
{
    if (!RVTVolumeComponent) return;

    // Recenter RVT volume on the camera tile
    FVector TileCenter = FWowCoordinate::TileToWorld(CameraTile.X, CameraTile.Y);
    RVTVolumeComponent->SetWorldLocation(FVector(TileCenter.X, TileCenter.Y, 0.0f));
}
