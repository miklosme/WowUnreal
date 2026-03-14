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

/** MLIQ liquid vertex (8 bytes): flow/UV data + height */
struct WOWDATA_API FWmoLiquidVertex
{
    float Height = 0.0f;
    uint8 Flow1 = 0;    // depth/flow for water; UV.x for magma (as int16 overlaid)
    uint8 Flow2 = 0;    // depth/flow for water; UV.y for magma (as int16 overlaid)
    uint8 FlowPct = 0;
    uint8 Filler = 0;
};

/** Parsed MLIQ liquid data from a WMO group */
struct WOWDATA_API FWmoLiquidData
{
    uint32 XVerts = 0;     // vertex grid width
    uint32 YVerts = 0;     // vertex grid height
    FVector Position = FVector::ZeroVector;  // base position offset
    uint16 MaterialId = 0; // liquid type/material
    TArray<FWmoLiquidVertex> Vertices; // XVerts * YVerts
    TArray<uint8> TileFlags;           // (XVerts-1) * (YVerts-1)
    bool HasLiquid() const { return XVerts > 1 && YVerts > 1 && Vertices.Num() > 0; }

    /** Get liquid category: 0=water, 1=ocean, 2=magma, 3=slime */
    int32 GetLiquidCategory() const
    {
        if (MaterialId <= 3) return 0;
        if (MaterialId <= 7) return 1;
        if (MaterialId <= 11) return 2;
        return 3;
    }
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
    FWmoLiquidData Liquid;
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
