#include "WowTerrainTile.h"
#include "WowTerrainMeshBuilder.h"
#include "WowTerrainMaterial.h"
#include "Formats/AdtTypes.h"
#include "Coord/WowCoordinate.h"
#include "WowDoodadManager.h"
#include "WowWmoRenderer.h"
#include "WowWaterRenderer.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerrainTile, Log, All);

AWowTerrainTile::AWowTerrainTile()
{
    PrimaryActorTick.bCanEverTick = false;
    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(RootScene);
}

UMaterialInterface* AWowTerrainTile::GetDefaultTerrainMaterial() const
{
    static UMaterial* DefaultMat = nullptr;
    if (!DefaultMat)
    {
        DefaultMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    }
    return DefaultMat;
}

void AWowTerrainTile::BuildFromAdtData(const FAdtData& Data, int32 TX, int32 TY, FMpqManager* Mpq, FWowAssetCache* Cache, TSet<uint32>* SpawnedWmoIds)
{
    TileCoord = FIntPoint(TX, TY);
    if (!Data.bIsValid) return;

    FVector TileCenter = FWowCoordinate::TileToWorld(TX, TY);
    SetActorLocation(TileCenter);

    UE_LOG(LogTerrainTile, Log, TEXT("Tile %d,%d at %s: %d textures, %d doodads, %d WMOs"),
        TX, TY, *TileCenter.ToString(), Data.TexturePaths.Num(), Data.DoodadPlacements.Num(), Data.WmoPlacements.Num());

    UMaterialInterface* DefaultMat = GetDefaultTerrainMaterial();

    // Create a single ProceduralMeshComponent with one section per chunk.
    // Each section gets its own material instance with BLP textures.
    UProceduralMeshComponent* MeshComp = NewObject<UProceduralMeshComponent>(this, TEXT("TerrainMesh"));
    MeshComp->SetupAttachment(RootScene);
    MeshComp->RegisterComponent();
    MeshComp->SetCastShadow(false);
    MeshComp->bUseComplexAsSimpleCollision = false;

    int32 SectionIndex = 0;
    int32 ChunksWithHeights = 0;
    int32 ChunksSkipped = 0;
    int32 ChunksTextured = 0;

    for (int32 i = 0; i < 256; ++i)
    {
        const FAdtChunkData& Chunk = Data.Chunks[i];
        FTerrainChunkMeshData MeshData = FTerrainMeshBuilder::BuildChunkMesh(Chunk, TX, TY);

        if (MeshData.Vertices.Num() == 0 || MeshData.Indices.Num() == 0)
        {
            ChunksSkipped++;
            continue;
        }

        // Check if this chunk has valid height variation
        bool bHasHeights = false;
        for (int32 h = 0; h < 145; h++)
        {
            if (Chunk.Heights[h] != 0.0f) { bHasHeights = true; break; }
        }
        if (bHasHeights) ChunksWithHeights++;

        // Build vertex colors and tangents for this chunk
        TArray<FLinearColor> Colors;
        TArray<FProcMeshTangent> Tangents;
        Colors.Reserve(MeshData.Vertices.Num());
        Tangents.Reserve(MeshData.Normals.Num());

        for (const FColor& C : MeshData.VertexColors)
        {
            Colors.Add(FLinearColor(C));
        }

        for (const FVector& N : MeshData.Normals)
        {
            FVector T = FVector::CrossProduct(N, FVector(0, 1, 0));
            if (T.SizeSquared() < 0.001f)
                T = FVector::CrossProduct(N, FVector(1, 0, 0));
            T.Normalize();
            Tangents.Add(FProcMeshTangent(T, false));
        }

        // Create mesh section for this chunk
        MeshComp->CreateMeshSection_LinearColor(
            SectionIndex, MeshData.Vertices, MeshData.Indices,
            MeshData.Normals, MeshData.UVs, Colors, Tangents, false);

        // Create a textured material for this chunk from BLP data
        UMaterialInstanceDynamic* ChunkMat = FWowTerrainMaterial::CreateChunkMaterial(
            Chunk, Data, Mpq, Cache, this);

        if (ChunkMat)
        {
            MeshComp->SetMaterial(SectionIndex, ChunkMat);
            ChunksTextured++;
        }
        else if (DefaultMat)
        {
            MeshComp->SetMaterial(SectionIndex, DefaultMat);
        }

        SectionIndex++;
    }

    if (SectionIndex > 0)
    {
        ChunkMeshes.Add(MeshComp);
        UE_LOG(LogTerrainTile, Log, TEXT("Tile %d,%d: %d sections, %d textured, %d/%d have heights, %d skipped"),
            TX, TY, SectionIndex, ChunksTextured, ChunksWithHeights, 256 - ChunksSkipped, ChunksSkipped);
    }
    else
    {
        MeshComp->DestroyComponent();
    }

    // Spawn water meshes from MH2O data
    {
        TArray<UProceduralMeshComponent*> WaterComps = FWowWaterRenderer::CreateWaterMeshes(this, Data, TX, TY);
        for (UProceduralMeshComponent* WC : WaterComps)
        {
            WaterMeshes.Add(WC);
        }
    }

    // Spawn doodads using HISMC instancing (batched per unique M2 model)
    if (Data.DoodadPlacements.Num() > 0)
    {
        InstancedDoodads = FWowDoodadManager::SpawnDoodadsInstanced(
            this, Data.DoodadPlacements, Data.DoodadPaths, Mpq, Cache);
        bUsesInstancedDoodads = (InstancedDoodads.Num() > 0);
    }

    // Store WMO placement data for distance-based streaming (doodads are now instanced)
    WmoPlacements = Data.WmoPlacements;
    WmoPaths = Data.WmoPaths;
    CachedMpq = Mpq;
    CachedCache = Cache;

    UE_LOG(LogTerrainTile, Log, TEXT("Tile %d,%d: %d instanced doodad HISMCs, %d WMO placements for streaming"),
        TX, TY, InstancedDoodads.Num(), WmoPlacements.Num());
}
