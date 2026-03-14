#include "WowTerrainMeshBuilder.h"
#include "Formats/AdtTypes.h"
#include "Coord/WowCoordinate.h"

/*
 * WoW terrain chunk vertex layout (145 vertices):
 *   Row 0: 9 outer vertices (indices 0-8)     -> Y=0
 *   Row 1: 8 inner vertices (indices 9-16)    -> Y=0.5
 *   Row 2: 9 outer vertices (indices 17-25)   -> Y=1
 *   Row 3: 8 inner vertices (indices 26-33)   -> Y=1.5
 *   ...
 *   Row 16: 9 outer vertices (indices 136-144) -> Y=8
 *
 * Outer rows have X positions at 0,1,...,8
 * Inner rows have X positions at 0.5,1.5,...,7.5
 *
 * Each quad (col,row) in the 8x8 grid is formed by 4 outer corners
 * plus 1 center inner vertex, creating 4 triangles in a fan pattern.
 *
 * The MCNK header stores the chunk's world position (WorldX, WorldY, WorldZ).
 * MCVT heights are ADDED to WorldZ to get the final vertex Z.
 * WorldX/WorldY define the chunk's northwest corner position.
 */

// Returns the vertex index in the 145-vertex array for an outer vertex at grid position (x, y)
// where x in [0..8], y in [0..8]
static int32 OuterIndex(int32 X, int32 Y)
{
    return Y * 17 + X;
}

// Returns the vertex index for an inner vertex at grid position (x, y)
// where x in [0..7], y in [0..7] (these sit at half-integer world positions)
static int32 InnerIndex(int32 X, int32 Y)
{
    return Y * 17 + 9 + X;
}

bool FTerrainMeshBuilder::IsHole(uint16 Holes, int32 QuadX, int32 QuadY)
{
    // The 4x4 hole grid maps to the 8x8 quad grid
    // Each hole bit covers a 2x2 block of quads
    int32 HoleX = QuadX / 2; // 0..3
    int32 HoleY = QuadY / 2; // 0..3
    int32 BitIndex = HoleY * 4 + HoleX;
    return (Holes & (1 << BitIndex)) != 0;
}

FTerrainChunkMeshData FTerrainMeshBuilder::BuildChunkMesh(const FAdtChunkData& ChunkData, int32 TileX, int32 TileY)
{
    FTerrainChunkMeshData Result;

    // Compute chunk position from tile + chunk indices (matching WoW ADT approach)
    // WoW ADT: xbase = tile.x * TILESIZE + chunk.ix * CHUNKSIZE (east-west)
    //          zbase = tile.z * TILESIZE + chunk.iy * CHUNKSIZE (north-south)
    // In WoW ADT coords: X=east, Y=up, Z=south
    // Our tile indices: TileX maps to WoW ADT tile.x, TileY maps to WoW ADT tile.z
    const float Xbase = TileX * FWowCoordinate::TILE_SIZE + ChunkData.IndexX * FWowCoordinate::CHUNK_SIZE;
    const float Zbase = TileY * FWowCoordinate::TILE_SIZE + ChunkData.IndexY * FWowCoordinate::CHUNK_SIZE;
    const float Ybase = ChunkData.WorldZ; // Height from MCNK header

    // Tile center in WoW ADT space for relative positioning
    const float TileCenterX = TileX * FWowCoordinate::TILE_SIZE + FWowCoordinate::TILE_SIZE * 0.5f;
    const float TileCenterZ = TileY * FWowCoordinate::TILE_SIZE + FWowCoordinate::TILE_SIZE * 0.5f;

    // Build 145 vertices
    Result.Vertices.SetNum(145);
    Result.Normals.SetNum(145);
    Result.UVs.SetNum(145);
    Result.AlphaUVs.SetNum(145);
    Result.VertexColors.SetNum(145);

    for (int32 i = 0; i < 145; ++i)
    {
        // Determine grid position of this vertex
        float GridX, GridY;

        // Figure out which row and column this vertex is in
        int32 Row = 0;
        int32 Col = 0;
        int32 Remaining = i;
        bool bIsInner = false;

        // Walk through rows: alternating 9 (outer) and 8 (inner)
        for (int32 R = 0; R < 17; ++R)
        {
            int32 RowSize = (R % 2 == 0) ? 9 : 8;
            if (Remaining < RowSize)
            {
                Row = R;
                Col = Remaining;
                bIsInner = (R % 2 == 1);
                break;
            }
            Remaining -= RowSize;
        }

        if (bIsInner)
        {
            // Inner vertex: half-integer positions
            GridX = Col + 0.5f;
            GridY = (Row / 2) + 0.5f;
        }
        else
        {
            // Outer vertex: integer positions
            GridX = (float)Col;
            GridY = (float)(Row / 2);
        }

        // WoW ADT vertex position: (xbase + xOffset, ybase + height, zbase + zOffset)
        // where xOffset = gridX * UNITSIZE, zOffset = gridY * 0.5 * UNITSIZE...
        // Actually WoW ADT iterates j(0..16)/i(0..8or7) and uses:
        //   xpos = i * UNITSIZE (+ 0.5*UNITSIZE if inner row)
        //   zpos = j * 0.5 * UNITSIZE
        // Our GridX/GridY already account for this offset pattern.
        float AdtX = Xbase + GridX * FWowCoordinate::UNIT_SIZE;  // east-west
        float AdtY = Ybase + ChunkData.Heights[i];                // up
        float AdtZ = Zbase + GridY * FWowCoordinate::UNIT_SIZE;  // north-south

        // Make relative to tile center
        float RelX = AdtX - TileCenterX;
        float RelZ = AdtZ - TileCenterZ;

        // Convert WoW ADT (X=east, Y=up, Z=south) to UE (X=forward, Y=right, Z=up)
        // UE.X = WoW ADT.Z * SCALE (south in WoW ADT = forward-ish)
        // UE.Y = WoW ADT.X * SCALE (east in WoW ADT = right)
        // UE.Z = WoW ADT.Y * SCALE (up = up)
        // But we need to negate to match WoW orientation properly
        Result.Vertices[i] = FVector(
            -RelZ * FWowCoordinate::SCALE,   // north-south (negate: WoW ADT Z=south, UE X=north)
            RelX * FWowCoordinate::SCALE,    // east-west
            AdtY * FWowCoordinate::SCALE  // height (absolute, not relative)
        );

        // MCNR normals stored as (east, north, up) in Normals[i].
        // For UE, just set all normals to UP for now — we can fix the
        // mapping to real MCNR normals later.
        // A flat surface should have normal (0, 0, 1) in UE.
        Result.Normals[i] = FVector(0, 0, 1);

        // Tiling UVs: 0..8 range across the chunk for texture tiling
        Result.UVs[i] = FVector2D(GridX, GridY);

        // Alpha map UVs: 0..1 range across the chunk
        Result.AlphaUVs[i] = FVector2D(GridX / 8.0f, GridY / 8.0f);

        // Vertex colors
        if (ChunkData.bHasVertexColors)
        {
            Result.VertexColors[i] = ChunkData.VertexColors[i];
        }
        else
        {
            Result.VertexColors[i] = FColor::White;
        }
    }

    // Build triangle indices (fan pattern per quad)
    // 8x8 quads, each with 4 triangles = 256 triangles max (minus holes)
    Result.Indices.Reserve(768); // 256 * 3

    for (int32 QY = 0; QY < 8; ++QY)
    {
        for (int32 QX = 0; QX < 8; ++QX)
        {
            // Skip hole quads
            if (IsHole(ChunkData.Holes, QX, QY))
            {
                continue;
            }

            // Outer corner vertex indices
            int32 TopLeft     = OuterIndex(QX,     QY);
            int32 TopRight    = OuterIndex(QX + 1, QY);
            int32 BottomLeft  = OuterIndex(QX,     QY + 1);
            int32 BottomRight = OuterIndex(QX + 1, QY + 1);

            // Center inner vertex
            int32 Center = InnerIndex(QX, QY);

            // 4 triangles in fan pattern around center vertex
            // CW winding when viewed from above = front face UP in UE's LH system
            Result.Indices.Add(TopLeft);
            Result.Indices.Add(Center);
            Result.Indices.Add(TopRight);

            Result.Indices.Add(TopRight);
            Result.Indices.Add(Center);
            Result.Indices.Add(BottomRight);

            Result.Indices.Add(BottomRight);
            Result.Indices.Add(Center);
            Result.Indices.Add(BottomLeft);

            Result.Indices.Add(BottomLeft);
            Result.Indices.Add(Center);
            Result.Indices.Add(TopLeft);
        }
    }

    return Result;
}

FTerrainChunkMeshData FTerrainMeshBuilder::BuildChunkMeshLOD1(const FAdtChunkData& ChunkData, int32 TileX, int32 TileY)
{
    FTerrainChunkMeshData Result;

    // Compute chunk position from tile + chunk indices (same as regular method)
    const float Xbase = TileX * FWowCoordinate::TILE_SIZE + ChunkData.IndexX * FWowCoordinate::CHUNK_SIZE;
    const float Zbase = TileY * FWowCoordinate::TILE_SIZE + ChunkData.IndexY * FWowCoordinate::CHUNK_SIZE;
    const float Ybase = ChunkData.WorldZ; // Height from MCNK header

    // Tile center in WoW ADT space for relative positioning
    const float TileCenterX = TileX * FWowCoordinate::TILE_SIZE + FWowCoordinate::TILE_SIZE * 0.5f;
    const float TileCenterZ = TileY * FWowCoordinate::TILE_SIZE + FWowCoordinate::TILE_SIZE * 0.5f;

    // Build only 81 outer vertices (9x9 grid)
    Result.Vertices.SetNum(81);
    Result.Normals.SetNum(81);
    Result.UVs.SetNum(81);
    Result.AlphaUVs.SetNum(81);
    Result.VertexColors.SetNum(81);

    int32 VertIdx = 0;
    for (int32 Row = 0; Row < 9; ++Row)
    {
        for (int32 Col = 0; Col < 9; ++Col)
        {
            // Use OuterIndex to read height from the original 145-height array
            int32 HeightIndex = OuterIndex(Col, Row);

            // Grid position for outer vertices (integer positions)
            float GridX = (float)Col;
            float GridY = (float)Row;

            // WoW ADT vertex position calculation (same as regular method)
            float AdtX = Xbase + GridX * FWowCoordinate::UNIT_SIZE;  // east-west
            float AdtY = Ybase + ChunkData.Heights[HeightIndex];     // up
            float AdtZ = Zbase + GridY * FWowCoordinate::UNIT_SIZE;  // north-south

            // Make relative to tile center
            float RelX = AdtX - TileCenterX;
            float RelZ = AdtZ - TileCenterZ;

            // Convert to UE coordinates (same as regular method)
            Result.Vertices[VertIdx] = FVector(
                -RelZ * FWowCoordinate::SCALE,   // north-south (negate: WoW ADT Z=south, UE X=north)
                RelX * FWowCoordinate::SCALE,    // east-west
                AdtY * FWowCoordinate::SCALE     // height (absolute, not relative)
            );

            // Set normal to UP for now
            Result.Normals[VertIdx] = FVector(0, 0, 1);

            // Tiling UVs: 0..8 range across the chunk for texture tiling
            Result.UVs[VertIdx] = FVector2D(GridX, GridY);

            // Alpha map UVs: 0..1 range across the chunk
            Result.AlphaUVs[VertIdx] = FVector2D(GridX / 8.0f, GridY / 8.0f);

            // Vertex colors
            if (ChunkData.bHasVertexColors)
            {
                Result.VertexColors[VertIdx] = ChunkData.VertexColors[HeightIndex];
            }
            else
            {
                Result.VertexColors[VertIdx] = FColor::White;
            }

            ++VertIdx;
        }
    }

    // Build triangle indices using simple 2-triangle-per-quad pattern
    // 8x8 quads, each with 2 triangles = 128 triangles max (minus holes)
    Result.Indices.Reserve(384); // 128 * 3

    for (int32 QY = 0; QY < 8; ++QY)
    {
        for (int32 QX = 0; QX < 8; ++QX)
        {
            // Skip hole quads
            if (IsHole(ChunkData.Holes, QX, QY))
            {
                continue;
            }

            // Vertex indices in the 81-vertex array (9x9 grid)
            int32 TopLeft     = QY * 9 + QX;
            int32 TopRight    = QY * 9 + QX + 1;
            int32 BottomLeft  = (QY + 1) * 9 + QX;
            int32 BottomRight = (QY + 1) * 9 + QX + 1;

            // First triangle: TL, BL, TR (CW winding)
            Result.Indices.Add(TopLeft);
            Result.Indices.Add(BottomLeft);
            Result.Indices.Add(TopRight);

            // Second triangle: TR, BL, BR (CW winding)
            Result.Indices.Add(TopRight);
            Result.Indices.Add(BottomLeft);
            Result.Indices.Add(BottomRight);
        }
    }

    return Result;
}
