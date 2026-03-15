#pragma once
#include "CoreMinimal.h"

struct FAdtChunkData;

struct FTerrainChunkMeshData
{
    TArray<FVector> Vertices;
    TArray<int32> Indices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;         // UV0: Alpha/splatmap UVs (0-1 per chunk)
    TArray<FVector2D> TiledUVs;    // UV1: World-space tiled UVs for texture sampling
    TArray<FColor> VertexColors;
};

class WOWWORLD_API FTerrainMeshBuilder
{
public:
    /** Build mesh data for a single terrain chunk */
    static FTerrainChunkMeshData BuildChunkMesh(const FAdtChunkData& ChunkData, int32 TileX, int32 TileY);

    /** Build simplified LOD 1 mesh using only outer 9x9 vertices (81 verts per chunk) */
    static FTerrainChunkMeshData BuildChunkMeshLOD1(const FAdtChunkData& ChunkData, int32 TileX, int32 TileY);

private:
    /** Check if a hole bit is set for the given quad coordinates */
    static bool IsHole(uint16 Holes, int32 QuadX, int32 QuadY);
};
