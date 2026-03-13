#pragma once
#include "CoreMinimal.h"

struct WOWDATA_API FWmoMaterial
{
    uint32 Flags = 0;
    uint32 Shader = 0;
    uint32 BlendMode = 0;
    FString TexturePath1;
    FString TexturePath2;
    FColor DiffuseColor = FColor::White;
    FColor EmissiveColor = FColor::Black;
};

struct WOWDATA_API FWmoGroupData
{
    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> TexCoords;
    TArray<uint16> Indices;
    TArray<FColor> VertexColors;
    struct FBatch { uint32 IndexStart=0; uint16 IndexCount=0; uint16 VertexStart=0; uint16 VertexEnd=0; uint8 MaterialIndex=0; };
    TArray<FBatch> Batches;
    FBox BoundingBox = FBox(ForceInit);
    uint32 Flags = 0;
    bool bIsValid = false;
};

struct WOWDATA_API FWmoRootData
{
    TArray<FWmoMaterial> Materials;
    uint32 NumGroups = 0;
    TArray<FString> DoodadPaths;
    FBox BoundingBox = FBox(ForceInit);
    uint32 Flags = 0;
    bool bIsValid = false;
};
