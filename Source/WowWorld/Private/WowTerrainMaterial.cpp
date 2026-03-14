#include "WowTerrainMaterial.h"
#include "Formats/AdtTypes.h"
#include "Formats/BlpParser.h"
#include "WowTextureFactory.h"
#include "WowAssetCache.h"
#include "Mpq/MpqManager.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogTerrainMat, Log, All);

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
            const FString& TexPath = AdtData.TexturePaths[TexIdx];
            UTexture2D* Tex = LoadBlpTexture(TexPath, Mpq, Cache);
            if (Tex)
            {
                UE_LOG(LogTerrainMat, Verbose, TEXT("Loaded layer %d texture: %s (%dx%d)"),
                    i, *TexPath, Tex->GetSizeX(), Tex->GetSizeY());
            }
            LayerTextures.Add(Tex);
        }
        else
        {
            LayerTextures.Add(nullptr);
        }
    }

    // If no textures at all, return nullptr to fall back to default
    if (LayerTextures.Num() == 0 || !LayerTextures[0])
    {
        UE_LOG(LogTerrainMat, Warning, TEXT("Chunk [%d,%d] has no valid base texture"),
            ChunkData.IndexX, ChunkData.IndexY);
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
    if (!BaseMat)
    {
        UE_LOG(LogTerrainMat, Error, TEXT("Failed to get base terrain material"));
        return nullptr;
    }

    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, Outer);
    if (!MID)
    {
        UE_LOG(LogTerrainMat, Error, TEXT("Failed to create MID"));
        return nullptr;
    }

    // Set the base layer texture (always present)
    MID->SetTextureParameterValue(FName(TEXT("BaseTexture")), LayerTextures[0]);
    UE_LOG(LogTerrainMat, Verbose, TEXT("Chunk [%d,%d]: assigned BaseTexture %s"),
        ChunkData.IndexX, ChunkData.IndexY, *LayerTextures[0]->GetName());

    // Set additional layer textures and their alpha maps
    static const FName LayerParamNames[] = {
        FName(TEXT("Layer1Texture")),
        FName(TEXT("Layer2Texture")),
        FName(TEXT("Layer3Texture"))
    };
    static const FName AlphaParamNames[] = {
        FName(TEXT("Alpha1")),
        FName(TEXT("Alpha2")),
        FName(TEXT("Alpha3"))
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
    static UMaterial* CachedMat = nullptr;
    if (CachedMat && CachedMat->IsValidLowLevel()) return CachedMat;

    // Try loading our custom material asset first
    CachedMat = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_WowTerrain"));
    if (CachedMat)
    {
        UE_LOG(LogTerrainMat, Log, TEXT("Loaded custom terrain material /Game/Materials/M_WowTerrain"));
        return CachedMat;
    }

#if WITH_EDITOR
    // No custom material found - create one programmatically with a BaseTexture parameter.
    // This uses editor-only APIs to build a material graph and compile it.
    UE_LOG(LogTerrainMat, Log, TEXT("Creating runtime terrain material with TextureSampleParameter2D"));

    CachedMat = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_WowTerrainRuntime"));
    CachedMat->SetShadingModel(MSM_DefaultLit);
    CachedMat->TwoSided = true;

    // Create a texture sample parameter named "BaseTexture"
    UMaterialExpressionTextureSampleParameter2D* TexParam =
        NewObject<UMaterialExpressionTextureSampleParameter2D>(CachedMat);
    TexParam->ParameterName = FName(TEXT("BaseTexture"));
    TexParam->SamplerType = SAMPLERTYPE_Color;
    // Default to white so the material compiles even without a texture set
    TexParam->Texture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture"));
    TexParam->MaterialExpressionEditorX = -400;
    TexParam->MaterialExpressionEditorY = 0;

    CachedMat->GetExpressionCollection().AddExpression(TexParam);

    // Connect the RGB output of the texture sample to BaseColor
    CachedMat->GetEditorOnlyData()->BaseColor.Connect(0, TexParam);

    // Trigger material compilation
    CachedMat->PreEditChange(nullptr);
    CachedMat->PostEditChange();

    UE_LOG(LogTerrainMat, Log, TEXT("Runtime terrain material created and compiled"));
#else
    // In non-editor builds, fall back to BasicShapeMaterial
    CachedMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    UE_LOG(LogTerrainMat, Warning, TEXT("No custom terrain material; using BasicShapeMaterial fallback"));
#endif

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
