#pragma once
#include "CoreMinimal.h"

class FMpqManager;
class FWowAssetCache;
struct FAdtChunkData;
struct FAdtData;

/**
 * Creates terrain materials with multi-layer texture splatting.
 * Each chunk gets a UMaterialInstanceDynamic with up to 4 ground textures
 * and 3 alpha blend maps.
 */
class WOWWORLD_API FWowTerrainMaterial
{
public:
    /**
     * Create a material instance for a terrain chunk.
     * Loads BLP textures from MPQ, creates alpha map textures,
     * and sets them as material parameters.
     */
    static UMaterialInstanceDynamic* CreateChunkMaterial(
        const FAdtChunkData& ChunkData,
        const FAdtData& AdtData,
        FMpqManager* Mpq,
        FWowAssetCache* Cache,
        UObject* Outer);

    /** Get or create the base terrain material */
    static UMaterial* GetBaseMaterial();

private:
    /** Create a UTexture2D from alpha map data (64x64 grayscale) */
    static UTexture2D* CreateAlphaTexture(const TArray<uint8>& AlphaData, const FString& Name);

    /** Load a BLP texture from MPQ, with caching */
    static UTexture2D* LoadBlpTexture(const FString& Path, FMpqManager* Mpq, FWowAssetCache* Cache);
};
