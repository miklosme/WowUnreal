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
    /** MCNK header world position (WoW coordinates) */
    float WorldX = 0.0f;
    float WorldY = 0.0f;
    float WorldZ = 0.0f;
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

/** A single liquid layer within a chunk */
struct WOWDATA_API FMH2OInstance
{
	uint16 LiquidType = 0;
	uint16 VertexFormat = 0;
	float MinHeight = 0.0f;
	float MaxHeight = 0.0f;
	uint8 XOffset = 0;
	uint8 YOffset = 0;
	uint8 Width = 8;
	uint8 Height = 8;
	/** Existence bitmap: bit (y*8+x) set means sub-tile has liquid */
	uint64 ExistsBitmap = 0;
	/** Height values: (Width+1)*(Height+1) floats */
	TArray<float> Heights;
	/** Depth values: (Width+1)*(Height+1) uint8 */
	TArray<uint8> Depths;

	/** 0=water, 1=ocean, 2=magma, 3=slime based on LiquidType ranges */
	int32 GetLiquidCategory() const
	{
		if (LiquidType <= 3) return 0; // water
		if (LiquidType <= 7) return 1; // ocean (or more water types)
		if (LiquidType <= 11) return 2; // magma
		return 3; // slime
	}
};

/** Water data for one chunk (up to multiple layers, usually 0-1) */
struct WOWDATA_API FMH2OChunkData
{
	TArray<FMH2OInstance> Layers;
	bool HasWater() const { return Layers.Num() > 0; }
};

struct WOWDATA_API FAdtData
{
    FAdtChunkData Chunks[256];
    TArray<FString> TexturePaths;
    TArray<FString> DoodadPaths;
    TArray<FString> WmoPaths;
    TArray<FAdtDoodadPlacement> DoodadPlacements;
    TArray<FAdtWmoPlacement> WmoPlacements;
    FMH2OChunkData WaterChunks[256];
    bool bBigAlpha = false;
    bool bIsValid = false;
};
