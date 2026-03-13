#pragma once
#include "CoreMinimal.h"

struct WOWDATA_API FM2Vertex
{
    FVector Position;
    FVector Normal;
    FVector2D TexCoord;
    FVector2D TexCoord2;
    uint8 BoneWeights[4] = {};
    uint8 BoneIndices[4] = {};
};

struct WOWDATA_API FM2RenderPass
{
    uint16 SubmeshIndex = 0;
    uint16 TextureIndex = 0;
    uint16 BlendMode = 0;
    uint16 Flags = 0;
};

struct WOWDATA_API FM2Data
{
    TArray<FM2Vertex> Vertices;
    TArray<uint16> Indices;
    TArray<FString> TexturePaths;
    TArray<FM2RenderPass> RenderPasses;
    FBox BoundingBox = FBox(ForceInit);
    float BoundingSphereRadius = 0.0f;
    uint32 NumBones = 0;
    bool bIsValid = false;
};
