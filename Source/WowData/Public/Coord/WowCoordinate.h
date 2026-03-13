#pragma once
#include "CoreMinimal.h"

struct WOWDATA_API FWowCoordinate
{
    static constexpr float TILE_SIZE = 533.33333f;
    static constexpr float CHUNK_SIZE = TILE_SIZE / 16.0f;
    static constexpr float UNIT_SIZE = CHUNK_SIZE / 8.0f;
    static constexpr float MAP_ORIGIN = 32.0f * TILE_SIZE;
    static constexpr float SCALE = 100.0f;

    static FVector WowToUE(float X, float Y, float Z) { return FVector(X * SCALE, -Y * SCALE, Z * SCALE); }
    static FVector WowToUE(const FVector& P) { return WowToUE(P.X, P.Y, P.Z); }
    static FVector UEToWow(const FVector& P) { return FVector(P.X / SCALE, -P.Y / SCALE, P.Z / SCALE); }

    static FVector TileToWorld(int32 TX, int32 TY)
    {
        float WX = MAP_ORIGIN - (TY * TILE_SIZE) - (TILE_SIZE * 0.5f);
        float WY = MAP_ORIGIN - (TX * TILE_SIZE) - (TILE_SIZE * 0.5f);
        return WowToUE(WX, WY, 0.0f);
    }

    static FIntPoint WorldToTile(const FVector& UEPos)
    {
        FVector W = UEToWow(UEPos);
        return FIntPoint(FMath::FloorToInt32((MAP_ORIGIN - W.Y) / TILE_SIZE), FMath::FloorToInt32((MAP_ORIGIN - W.X) / TILE_SIZE));
    }

    static FVector ChunkWorldPosition(int32 TX, int32 TY, int32 CX, int32 CY)
    {
        float WX = MAP_ORIGIN - (TY * TILE_SIZE) - (CY * CHUNK_SIZE);
        float WY = MAP_ORIGIN - (TX * TILE_SIZE) - (CX * CHUNK_SIZE);
        return WowToUE(WX, WY, 0.0f);
    }

    static FRotator WowRotationToUE(float RX, float RY, float RZ) { return FRotator(RX, -RZ, RY); }
};
