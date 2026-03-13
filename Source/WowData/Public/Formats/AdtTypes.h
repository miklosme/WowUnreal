#pragma once
#include "CoreMinimal.h"

struct WOWDATA_API FAdtChunkData
{
    float Heights[145] = {};
    FVector Normals[145];
    FColor VertexColors[145];
    TArray<int32> TextureIndices;
    TArray<TArray<uint8>> AlphaMaps;
    int32 IndexX = 0;
    int32 IndexY = 0;
    uint32 AreaId = 0;
    uint16 Holes = 0;
    bool bHasVertexColors = false;
};

struct WOWDATA_API FAdtDoodadPlacement
{
    int32 NameIndex = 0;
    uint32 UniqueId = 0;
    FVector Position = FVector::ZeroVector;
    FVector Rotation = FVector::ZeroVector;
    uint16 Scale = 1024;
    uint16 Flags = 0;
    float GetScaleFloat() const { return Scale == 0 ? 1.0f : Scale / 1024.0f; }
};

struct WOWDATA_API FAdtWmoPlacement
{
    int32 NameIndex = 0;
    uint32 UniqueId = 0;
    FVector Position = FVector::ZeroVector;
    FVector Rotation = FVector::ZeroVector;
    FBox BoundingBox = FBox(ForceInit);
    uint16 Flags = 0;
    uint16 DoodadSet = 0;
    uint16 NameSet = 0;
    uint16 Scale = 1024;
};

struct WOWDATA_API FAdtData
{
    FAdtChunkData Chunks[256];
    TArray<FString> TexturePaths;
    TArray<FString> DoodadPaths;
    TArray<FString> WmoPaths;
    TArray<FAdtDoodadPlacement> DoodadPlacements;
    TArray<FAdtWmoPlacement> WmoPlacements;
    bool bBigAlpha = false;
    bool bIsValid = false;
};
