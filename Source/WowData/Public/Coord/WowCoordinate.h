#pragma once
#include "CoreMinimal.h"

/**
 * Coordinate conversion between WoW/noggit3 space and UE space.
 *
 * Noggit3 coordinate system (used by ADT data):
 *   X = east (increases going east), range: 0 to 64*TILESIZE
 *   Y = up (height)
 *   Z = south (increases going south), range: 0 to 64*TILESIZE
 *   Tile (0,0) is at the northwest corner
 *
 * UE coordinate system:
 *   X = north (we map -noggit3.Z to UE.X so north is positive)
 *   Y = east (we map noggit3.X to UE.Y)
 *   Z = up (we map noggit3.Y to UE.Z)
 *   Scale: 1 WoW unit ~ 1 yard ~ 91.44cm, we use SCALE=100 for simplicity
 */
struct WOWDATA_API FWowCoordinate
{
    static constexpr float TILE_SIZE = 533.33333f;
    static constexpr float CHUNK_SIZE = TILE_SIZE / 16.0f;
    static constexpr float UNIT_SIZE = CHUNK_SIZE / 8.0f;
    static constexpr float MAP_ORIGIN = 32.0f * TILE_SIZE;
    static constexpr float SCALE = 100.0f;

    /** Convert noggit3 coords (X=east, Y=up, Z=south) to UE coords */
    static FVector NoggitToUE(float NgX, float NgY, float NgZ)
    {
        return FVector(-NgZ * SCALE, NgX * SCALE, NgY * SCALE);
    }

    /** Get UE world position for the center of a tile (for placing the tile actor) */
    static FVector TileToWorld(int32 TileX, int32 TileY)
    {
        float NgX = TileX * TILE_SIZE + TILE_SIZE * 0.5f;  // east-west center
        float NgZ = TileY * TILE_SIZE + TILE_SIZE * 0.5f;  // north-south center
        return NoggitToUE(NgX, 0.0f, NgZ);
    }

    /** Get the tile indices for a given UE world position */
    static FIntPoint WorldToTile(const FVector& UEPos)
    {
        // Reverse of NoggitToUE: NgZ = -UE.X / SCALE, NgX = UE.Y / SCALE
        float NgX = UEPos.Y / SCALE;
        float NgZ = -UEPos.X / SCALE;
        int32 TX = FMath::FloorToInt32(NgX / TILE_SIZE);
        int32 TY = FMath::FloorToInt32(NgZ / TILE_SIZE);
        return FIntPoint(TX, TY);
    }

    /** Convert WoW world coords (X=north, Y=west, Z=up) to UE coords */
    static FVector WowToUE(float X, float Y, float Z) { return FVector(X * SCALE, -Y * SCALE, Z * SCALE); }
    static FVector WowToUE(const FVector& P) { return WowToUE(P.X, P.Y, P.Z); }
    static FVector UEToWow(const FVector& P) { return FVector(P.X / SCALE, -P.Y / SCALE, P.Z / SCALE); }

    static FRotator WowRotationToUE(float RX, float RY, float RZ) { return FRotator(RX, -RZ, RY); }
};
