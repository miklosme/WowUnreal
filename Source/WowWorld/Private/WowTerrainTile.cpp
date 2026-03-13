#include "WowTerrainTile.h"
#include "WowTerrainMeshBuilder.h"
#include "Formats/AdtTypes.h"
#include "Coord/WowCoordinate.h"

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

    // Don't set actor location to tile center - vertices are already in absolute world space
    // (WowToUE produces absolute positions from WoW world coordinates)

    UE_LOG(LogTemp, Log, TEXT("Tile %d,%d: %d textures, %d doodads, %d WMOs"),
        TX, TY, Data.TexturePaths.Num(), Data.DoodadPlacements.Num(), Data.WmoPlacements.Num());

    UMaterialInterface* TerrainMat = GetDefaultTerrainMaterial();

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

        if (TerrainMat)
        {
            MeshComp->SetMaterial(0, TerrainMat);
        }

        MeshComp->SetCastShadow(true);

        ChunkMeshes.Add(MeshComp);
        ++ChunksBuilt;
    }

    UE_LOG(LogTemp, Log, TEXT("Tile %d,%d: built %d chunk meshes"), TX, TY, ChunksBuilt);
}
