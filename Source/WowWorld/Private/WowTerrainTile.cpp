#include "WowTerrainTile.h"
#include "WowTerrainMeshBuilder.h"
#include "WowTerrainMaterial.h"
#include "Formats/AdtTypes.h"
#include "Coord/WowCoordinate.h"
#include "WowDoodadManager.h"
#include "WowWmoRenderer.h"

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

    // Merge all 256 chunks into a SINGLE mesh to eliminate gaps
    TArray<FVector> AllVertices;
    TArray<int32> AllIndices;
    TArray<FVector> AllNormals;
    TArray<FVector2D> AllUVs;
    TArray<FLinearColor> AllColors;
    TArray<FProcMeshTangent> AllTangents;

    // Reserve space: 256 chunks * 145 verts, 256 * 768 indices
    AllVertices.Reserve(256 * 145);
    AllIndices.Reserve(256 * 768);
    AllNormals.Reserve(256 * 145);
    AllUVs.Reserve(256 * 145);
    AllColors.Reserve(256 * 145);
    AllTangents.Reserve(256 * 145);

    int32 VertexOffset = 0;
    int32 ChunksWithHeights = 0;
    int32 ChunksSkipped = 0;

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

        // Append vertices
        AllVertices.Append(MeshData.Vertices);
        AllNormals.Append(MeshData.Normals);
        AllUVs.Append(MeshData.UVs);

        // Vertex colors
        for (const FColor& C : MeshData.VertexColors)
        {
            AllColors.Add(FLinearColor(C));
        }

        // Tangents
        for (const FVector& N : MeshData.Normals)
        {
            FVector T = FVector::CrossProduct(N, FVector(0, 1, 0));
            if (T.SizeSquared() < 0.001f)
                T = FVector::CrossProduct(N, FVector(1, 0, 0));
            T.Normalize();
            AllTangents.Add(FProcMeshTangent(T, false));
        }

        // Offset indices for merged buffer
        for (int32 Idx : MeshData.Indices)
        {
            AllIndices.Add(Idx + VertexOffset);
        }

        VertexOffset += MeshData.Vertices.Num();
    }

    if (AllVertices.Num() > 0)
    {
        // Create single merged mesh component
        UProceduralMeshComponent* MeshComp = NewObject<UProceduralMeshComponent>(this, TEXT("TerrainMesh"));
        MeshComp->SetupAttachment(RootScene);
        MeshComp->RegisterComponent();

        MeshComp->CreateMeshSection_LinearColor(
            0, AllVertices, AllIndices, AllNormals, AllUVs, AllColors, AllTangents, false);

        // Apply material
        if (DefaultMat)
        {
            MeshComp->SetMaterial(0, DefaultMat);
        }

        MeshComp->SetCastShadow(false);
        MeshComp->bUseComplexAsSimpleCollision = false;
        // Render both sides to avoid backface culling gaps
        MeshComp->SetRenderCustomDepth(false);
        ChunkMeshes.Add(MeshComp);

        // Log bounds for gap debugging
        FVector MinV(FLT_MAX), MaxV(-FLT_MAX);
        for (const FVector& V : AllVertices)
        {
            MinV.X = FMath::Min(MinV.X, V.X); MinV.Y = FMath::Min(MinV.Y, V.Y); MinV.Z = FMath::Min(MinV.Z, V.Z);
            MaxV.X = FMath::Max(MaxV.X, V.X); MaxV.Y = FMath::Max(MaxV.Y, V.Y); MaxV.Z = FMath::Max(MaxV.Z, V.Z);
        }
        FVector WorldMin = GetActorLocation() + MinV;
        FVector WorldMax = GetActorLocation() + MaxV;
        UE_LOG(LogTerrainTile, Log, TEXT("Tile %d,%d: %d verts, %d tris, %d/%d chunks have heights, %d skipped. World[%s to %s]"),
            TX, TY, AllVertices.Num(), AllIndices.Num() / 3, ChunksWithHeights, 256 - ChunksSkipped, ChunksSkipped,
            *WorldMin.ToString(), *WorldMax.ToString());
    }

    // TODO: Doodads temporarily disabled - too many ProceduralMesh components crash Metal
    // Need to batch into HISMC or use runtime StaticMesh
    // FWowDoodadManager::SpawnDoodads(this, Data.DoodadPlacements, Data.DoodadPaths, Mpq, Cache);

    // TODO: WMOs temporarily disabled - too many ProceduralMesh components crash Metal
    // Need to batch group meshes or use runtime StaticMesh
    int32 WmosSpawned = 0;
    if (false) // DISABLED
    for (const FAdtWmoPlacement& WmoPlacement : Data.WmoPlacements)
    {
        // Skip WMOs already spawned by another tile
        if (SpawnedWmoIds)
        {
            if (SpawnedWmoIds->Contains(WmoPlacement.UniqueId))
                continue;
            SpawnedWmoIds->Add(WmoPlacement.UniqueId);
        }

        if (WmoPlacement.NameIndex >= 0 && WmoPlacement.NameIndex < Data.WmoPaths.Num())
        {
            const FString& WmoPath = Data.WmoPaths[WmoPlacement.NameIndex];
            if (!WmoPath.IsEmpty())
            {
                AActor* WmoActor = FWowWmoRenderer::SpawnWmo(GetWorld(), WmoPath, WmoPlacement, Mpq, Cache);
                if (WmoActor)
                {
                    WmoActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
                    ++WmosSpawned;
                }
            }
        }
    }

    UE_LOG(LogTerrainTile, Log, TEXT("Tile %d,%d: %d WMOs spawned"), TX, TY, WmosSpawned);
}
