#include "WowTerrainMaterial.h"
#include "Formats/AdtTypes.h"
#include "Formats/BlpParser.h"
#include "WowTextureFactory.h"
#include "WowAssetCache.h"
#include "Mpq/MpqManager.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"

UMaterialInstanceDynamic* FWowTerrainMaterial::CreateChunkMaterial(
    const FAdtChunkData& ChunkData,
    const FAdtData& AdtData,
    FMpqManager* Mpq,
    FWowAssetCache* Cache,
    UObject* Outer)
{
    if (!Mpq || !Outer) return nullptr;

    // Load ground textures referenced by this chunk
    TArray<UTexture2D*> LayerTextures;
    for (int32 i = 0; i < ChunkData.TextureIndices.Num() && i < 4; ++i)
    {
        int32 TexIdx = ChunkData.TextureIndices[i];
        if (AdtData.TexturePaths.IsValidIndex(TexIdx))
        {
            UTexture2D* Tex = LoadBlpTexture(AdtData.TexturePaths[TexIdx], Mpq, Cache);
            LayerTextures.Add(Tex);
        }
        else
        {
            LayerTextures.Add(nullptr);
        }
    }

    // If no textures at all, return nullptr to fall back to default
    if (LayerTextures.Num() == 0 || (LayerTextures.Num() > 0 && !LayerTextures[0]))
    {
        return nullptr;
    }

    // Create alpha map textures from chunk data (layers 1-3 have alpha maps)
    TArray<UTexture2D*> AlphaTextures;
    for (int32 i = 0; i < ChunkData.AlphaMaps.Num() && i < 3; ++i)
    {
        if (ChunkData.AlphaMaps[i].Num() > 0)
        {
            FString AlphaName = FString::Printf(TEXT("Alpha_%d_%d_%d"), ChunkData.IndexX, ChunkData.IndexY, i);
            UTexture2D* Alpha = CreateAlphaTexture(ChunkData.AlphaMaps[i], AlphaName);
            AlphaTextures.Add(Alpha);
        }
        else
        {
            AlphaTextures.Add(nullptr);
        }
    }

    // Get base material and create dynamic instance
    UMaterial* BaseMat = GetBaseMaterial();
    if (!BaseMat) return nullptr;

    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, Outer);
    if (!MID) return nullptr;

    // Set the base layer texture (always present)
    if (LayerTextures.Num() > 0 && LayerTextures[0])
    {
        MID->SetTextureParameterValue(TEXT("BaseTexture"), LayerTextures[0]);
    }

    // Set additional layer textures and their alpha maps
    static const FName LayerParamNames[] = {
        TEXT("Layer1Texture"),
        TEXT("Layer2Texture"),
        TEXT("Layer3Texture")
    };
    static const FName AlphaParamNames[] = {
        TEXT("Alpha1"),
        TEXT("Alpha2"),
        TEXT("Alpha3")
    };

    for (int32 i = 0; i < 3; ++i)
    {
        if (i + 1 < LayerTextures.Num() && LayerTextures[i + 1])
        {
            MID->SetTextureParameterValue(LayerParamNames[i], LayerTextures[i + 1]);
        }
        if (i < AlphaTextures.Num() && AlphaTextures[i])
        {
            MID->SetTextureParameterValue(AlphaParamNames[i], AlphaTextures[i]);
        }
    }

    return MID;
}

UMaterial* FWowTerrainMaterial::GetBaseMaterial()
{
    // Try to load our custom terrain splatting material first.
    // If it doesn't exist yet, fall back to BasicShapeMaterial which
    // has a texture parameter we can use for the base layer.
    static UMaterial* CachedMat = nullptr;
    if (CachedMat) return CachedMat;

    // Try our custom material first
    CachedMat = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_WowTerrain"));
    if (CachedMat) return CachedMat;

    // Fall back to engine material that supports a texture parameter
    CachedMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    return CachedMat;
}

UTexture2D* FWowTerrainMaterial::CreateAlphaTexture(const TArray<uint8>& AlphaData, const FString& Name)
{
    // Alpha maps in WoW ADTs are 64x64 grayscale
    const int32 AlphaSize = 64;
    const int32 ExpectedBytes = AlphaSize * AlphaSize;

    if (AlphaData.Num() < ExpectedBytes)
    {
        UE_LOG(LogTemp, Warning, TEXT("Alpha map '%s' has %d bytes, expected %d"), *Name, AlphaData.Num(), ExpectedBytes);
        return nullptr;
    }

    UTexture2D* Tex = UTexture2D::CreateTransient(AlphaSize, AlphaSize, PF_G8, *Name);
    if (!Tex) return nullptr;

    Tex->Filter = TF_Bilinear;
    Tex->SRGB = false; // Alpha data is linear
    Tex->NeverStream = true;
    Tex->LODGroup = TEXTUREGROUP_Terrain_Weightmap;
    Tex->AddressX = TA_Clamp;
    Tex->AddressY = TA_Clamp;

    void* TexData = Tex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
    if (TexData)
    {
        FMemory::Memcpy(TexData, AlphaData.GetData(), ExpectedBytes);
        Tex->GetPlatformData()->Mips[0].BulkData.Unlock();
    }
    Tex->UpdateResource();

    return Tex;
}

UTexture2D* FWowTerrainMaterial::LoadBlpTexture(const FString& Path, FMpqManager* Mpq, FWowAssetCache* Cache)
{
    if (Path.IsEmpty() || !Mpq) return nullptr;

    // Check cache first
    if (Cache)
    {
        UTexture2D* Cached = Cache->FindTexture(Path);
        if (Cached) return Cached;
    }

    // Read BLP file from MPQ
    TArray<uint8> FileData;
    if (!Mpq->ReadFile(Path, FileData))
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to read texture from MPQ: %s"), *Path);
        return nullptr;
    }

    // Parse BLP
    FBlpTexture BlpData = FBlpParser::Parse(FileData);
    if (!BlpData.bIsValid)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to parse BLP texture: %s"), *Path);
        return nullptr;
    }

    // Create UTexture2D
    FString TexName = FPaths::GetBaseFilename(Path);
    UTexture2D* Tex = FWowTextureFactory::CreateTexture(BlpData, TexName);
    if (!Tex)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to create texture: %s"), *Path);
        return nullptr;
    }

    // Terrain textures should tile
    Tex->AddressX = TA_Wrap;
    Tex->AddressY = TA_Wrap;

    // Cache it
    if (Cache)
    {
        Cache->CacheTexture(Path, Tex);
    }

    return Tex;
}
