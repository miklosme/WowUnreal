#include "WowTerrainTile.h"
#include "WowTerrainMeshBuilder.h"
#include "WowTerrainMaterial.h"
#include "Formats/AdtTypes.h"
#include "Coord/WowCoordinate.h"
#include "WowDoodadManager.h"
#include "WowWmoRenderer.h"

AWowTerrainTile::AWowTerrainTile()
{
    PrimaryActorTick.bCanEverTick = false;
    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(RootScene);
}

UMaterialInterface* AWowTerrainTile::GetDefaultTerrainMaterial() const
{
    // Try to load a simple opaque material; fall back to engine default
    static UMaterial* DefaultMat = nullptr;
    if (!DefaultMat)
    {
        DefaultMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    }
    return DefaultMat;
}

void AWowTerrainTile::BuildFromAdtData(const FAdtData& Data, int32 TX, int32 TY, FMpqManager* Mpq, FWowAssetCache* Cache)
{
    TileCoord = FIntPoint(TX, TY);
    if (!Data.bIsValid) return;

    // Set actor to tile center, vertices will be in local space relative to this
    FVector TileCenter = FWowCoordinate::TileToWorld(TX, TY);
    SetActorLocation(TileCenter);

    UE_LOG(LogTemp, Log, TEXT("Tile %d,%d at world pos %s: %d textures, %d doodads, %d WMOs"),
        TX, TY, *TileCenter.ToString(), Data.TexturePaths.Num(), Data.DoodadPlacements.Num(), Data.WmoPlacements.Num());

    UMaterialInterface* DefaultMat = GetDefaultTerrainMaterial();

    // Build mesh for each of the 256 chunks (16x16 grid)
    int32 ChunksBuilt = 0;
    for (int32 i = 0; i < 256; ++i)
    {
        const FAdtChunkData& Chunk = Data.Chunks[i];

        FTerrainChunkMeshData MeshData = FTerrainMeshBuilder::BuildChunkMesh(Chunk, TX, TY);

        if (MeshData.Vertices.Num() == 0 || MeshData.Indices.Num() == 0)
        {
            continue;
        }

        // Create procedural mesh component for this chunk
        FName CompName = *FString::Printf(TEXT("Chunk_%d_%d"), Chunk.IndexX, Chunk.IndexY);
        UProceduralMeshComponent* MeshComp = NewObject<UProceduralMeshComponent>(this, CompName);
        MeshComp->SetupAttachment(RootScene);
        MeshComp->RegisterComponent();

        // Convert data to the format ProceduralMeshComponent expects
        TArray<FVector2D> EmptyUV;
        TArray<FLinearColor> EmptyVertColors;
        TArray<FProcMeshTangent> Tangents;

        // Generate tangents from normals (approximate)
        Tangents.SetNum(MeshData.Normals.Num());
        for (int32 V = 0; V < MeshData.Normals.Num(); ++V)
        {
            FVector N = MeshData.Normals[V];
            // Create tangent perpendicular to normal in the XZ plane
            FVector T = FVector::CrossProduct(N, FVector(0, 1, 0));
            if (T.SizeSquared() < 0.001f)
            {
                T = FVector::CrossProduct(N, FVector(1, 0, 0));
            }
            T.Normalize();
            Tangents[V] = FProcMeshTangent(T, false);
        }

        // Convert FColor to FLinearColor for vertex colors
        TArray<FLinearColor> LinearVertColors;
        LinearVertColors.SetNum(MeshData.VertexColors.Num());
        for (int32 V = 0; V < MeshData.VertexColors.Num(); ++V)
        {
            LinearVertColors[V] = FLinearColor(MeshData.VertexColors[V]);
        }

        MeshComp->CreateMeshSection_LinearColor(
            0,                       // Section index
            MeshData.Vertices,       // Vertices
            MeshData.Indices,        // Triangles
            MeshData.Normals,        // Normals
            MeshData.UVs,            // UV0 (tiling)
            LinearVertColors,        // Vertex colors
            Tangents,                // Tangents
            true                     // Create collision
        );

        // Log first chunk's first vertex for debugging
        if (i == 0 && MeshData.Vertices.Num() > 0)
        {
            UE_LOG(LogTemp, Log, TEXT("Tile %d,%d chunk 0 vertex 0: %s (local), actor at %s"),
                TX, TY, *MeshData.Vertices[0].ToString(), *GetActorLocation().ToString());
            UE_LOG(LogTemp, Log, TEXT("  Chunk 0: %d verts, %d indices, WowPos(%f, %f, %f)"),
                MeshData.Vertices.Num(), MeshData.Indices.Num(),
                Chunk.WorldX, Chunk.WorldY, Chunk.WorldZ);
        }

        // Try to create a textured material from BLP data; fall back to default
        UMaterialInstanceDynamic* ChunkMaterial = FWowTerrainMaterial::CreateChunkMaterial(
            Chunk, Data, Mpq, Cache, this);

        if (ChunkMaterial)
        {
            MeshComp->SetMaterial(0, ChunkMaterial);
        }
        else if (DefaultMat)
        {
            MeshComp->SetMaterial(0, DefaultMat);
        }

        MeshComp->SetCastShadow(true);

        ChunkMeshes.Add(MeshComp);
        ++ChunksBuilt;
    }

    UE_LOG(LogTemp, Log, TEXT("Tile %d,%d: built %d chunk meshes"), TX, TY, ChunksBuilt);

    // Spawn doodads (M2 models)
    if (Data.DoodadPlacements.Num() > 0 && Data.DoodadPaths.Num() > 0)
    {
        FWowDoodadManager::SpawnDoodads(this, Data.DoodadPlacements, Data.DoodadPaths, Mpq, Cache);
    }

    // Spawn WMOs (buildings/structures)
    int32 WmosSpawned = 0;
    for (const FAdtWmoPlacement& WmoPlacement : Data.WmoPlacements)
    {
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
    if (WmosSpawned > 0)
    {
        UE_LOG(LogTemp, Log, TEXT("Tile %d,%d: spawned %d WMOs"), TX, TY, WmosSpawned);
    }
}
