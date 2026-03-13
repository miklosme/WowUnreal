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

    // Use the MCNK header's world position (stored by parser)
    // WorldX = north position (WoW X axis), WorldY = west position (WoW Y axis)
    // WorldZ = base height that MCVT values are relative to
    const float ChunkWorldX = ChunkData.WorldX;
    const float ChunkWorldY = ChunkData.WorldY;
    const float ChunkWorldZ = ChunkData.WorldZ;

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

        // WoW world position for this vertex
        // In WoW, X decreases going south and Y decreases going east
        // The grid extends from the chunk origin going south (decreasing X) and east (decreasing Y)
        float WowX = ChunkWorldX - (GridY * FWowCoordinate::UNIT_SIZE);
        float WowY = ChunkWorldY - (GridX * FWowCoordinate::UNIT_SIZE);
        float WowZ = ChunkWorldZ + ChunkData.Heights[i]; // Heights are relative to base Z

        // Convert to UE coordinates
        Result.Vertices[i] = FWowCoordinate::WowToUE(WowX, WowY, WowZ);

        // Convert normal from WoW space to UE space
        // WoW normals are in (X, Y, Z) WoW space; apply same transform as WowToUE
        const FVector& WowNormal = ChunkData.Normals[i];
        Result.Normals[i] = FVector(WowNormal.X, -WowNormal.Y, WowNormal.Z);
        if (Result.Normals[i].IsNearlyZero())
        {
            Result.Normals[i] = FVector(0, 0, 1);
        }

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

            // 4 triangles in fan pattern around center
            // Winding order: counter-clockwise when viewed from above (UE convention)
            // Note: WowToUE negates Y, which flips handedness, so we use
            // the opposite winding compared to WoW's native order.

            // Triangle 1: Top edge
            Result.Indices.Add(TopLeft);
            Result.Indices.Add(TopRight);
            Result.Indices.Add(Center);

            // Triangle 2: Right edge
            Result.Indices.Add(TopRight);
            Result.Indices.Add(BottomRight);
            Result.Indices.Add(Center);

            // Triangle 3: Bottom edge
            Result.Indices.Add(BottomRight);
            Result.Indices.Add(BottomLeft);
            Result.Indices.Add(Center);

            // Triangle 4: Left edge
            Result.Indices.Add(BottomLeft);
            Result.Indices.Add(TopLeft);
            Result.Indices.Add(Center);
        }
    }

    return Result;
}
