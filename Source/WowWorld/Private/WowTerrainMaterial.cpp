#include "WowTerrainMaterial.h"
#include "Formats/AdtTypes.h"
#include "Formats/BlpParser.h"
#include "WowTextureFactory.h"
#include "WowAssetCache.h"
#include "Mpq/MpqManager.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/PackageName.h"
#if WITH_EDITOR
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
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

    // Try loading a hand-authored material asset first (if it exists)
    {
        FSoftObjectPath MatPath(TEXT("/Game/Materials/M_WowTerrain.M_WowTerrain"));
        if (MatPath.ResolveObject() || FPackageName::DoesPackageExist(MatPath.GetLongPackageName()))
        {
            CachedMat = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_WowTerrain"));
        }
    }
    if (CachedMat)
    {
        UE_LOG(LogTerrainMat, Log, TEXT("Loaded custom terrain material /Game/Materials/M_WowTerrain"));
        return CachedMat;
    }

#if WITH_EDITOR
    // Build the full 4-layer terrain splat material graph programmatically.
    // Layout:
    //   UV0 (0..8 tiling) → ground texture samplers
    //   UV0 / 8.0 (0..1)  → alpha map samplers
    //   Lerp chain: Base → Layer1 (Alpha1) → Layer2 (Alpha2) → Layer3 (Alpha3) → BaseColor
    UE_LOG(LogTerrainMat, Log, TEXT("Creating 4-layer terrain splat material"));

    CachedMat = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_WowTerrainRuntime"));
    CachedMat->SetShadingModel(MSM_DefaultLit);
    CachedMat->TwoSided = false;

    UTexture2D* WhiteTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture"));
    UTexture2D* BlackTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/Black"));

    auto& Exprs = CachedMat->GetExpressionCollection();

    // --- TexCoord node (UV0) for alpha map UV computation ---
    auto* TexCoord = NewObject<UMaterialExpressionTextureCoordinate>(CachedMat);
    TexCoord->CoordinateIndex = 0;
    TexCoord->MaterialExpressionEditorX = -900;
    TexCoord->MaterialExpressionEditorY = 600;
    Exprs.AddExpression(TexCoord);

    // --- Divide UV0 by 8.0 to get alpha UVs (0..1 range) ---
    auto* DivConst = NewObject<UMaterialExpressionConstant>(CachedMat);
    DivConst->R = 8.0f;
    DivConst->MaterialExpressionEditorX = -900;
    DivConst->MaterialExpressionEditorY = 700;
    Exprs.AddExpression(DivConst);

    auto* AlphaUVDiv = NewObject<UMaterialExpressionDivide>(CachedMat);
    AlphaUVDiv->MaterialExpressionEditorX = -750;
    AlphaUVDiv->MaterialExpressionEditorY = 600;
    AlphaUVDiv->A.Connect(0, TexCoord);
    AlphaUVDiv->B.Connect(0, DivConst);
    Exprs.AddExpression(AlphaUVDiv);

    // --- 4 ground texture samplers (use default UV0 for tiling) ---
    const TCHAR* GroundParamNames[] = {
        TEXT("BaseTexture"), TEXT("Layer1Texture"), TEXT("Layer2Texture"), TEXT("Layer3Texture")
    };
    UMaterialExpressionTextureSampleParameter2D* GroundSamplers[4];

    for (int32 i = 0; i < 4; ++i)
    {
        auto* Sampler = NewObject<UMaterialExpressionTextureSampleParameter2D>(CachedMat);
        Sampler->ParameterName = FName(GroundParamNames[i]);
        Sampler->SamplerType = SAMPLERTYPE_Color;
        Sampler->Texture = (i == 0) ? WhiteTex : BlackTex;
        Sampler->MaterialExpressionEditorX = -600;
        Sampler->MaterialExpressionEditorY = i * 200;
        Exprs.AddExpression(Sampler);
        GroundSamplers[i] = Sampler;
    }

    // --- 3 alpha map samplers (sample with computed alpha UVs) ---
    const TCHAR* AlphaParamNames[] = {
        TEXT("Alpha1"), TEXT("Alpha2"), TEXT("Alpha3")
    };
    UMaterialExpressionTextureSampleParameter2D* AlphaSamplers[3];
    UMaterialExpressionComponentMask* AlphaMasks[3];

    for (int32 i = 0; i < 3; ++i)
    {
        auto* Sampler = NewObject<UMaterialExpressionTextureSampleParameter2D>(CachedMat);
        Sampler->ParameterName = FName(AlphaParamNames[i]);
        // Use a color sampler with the engine's default black texture; Metal rejects
        // grayscale samplers when the default texture asset is authored as color.
        Sampler->SamplerType = SAMPLERTYPE_Color;
        Sampler->Texture = BlackTex; // Default black = no blending
        Sampler->MaterialExpressionEditorX = -600;
        Sampler->MaterialExpressionEditorY = 800 + i * 200;
        // Wire alpha UVs into the sampler
        Sampler->Coordinates.Connect(0, AlphaUVDiv);
        Exprs.AddExpression(Sampler);
        AlphaSamplers[i] = Sampler;

        auto* Mask = NewObject<UMaterialExpressionComponentMask>(CachedMat);
        Mask->R = true;
        Mask->G = false;
        Mask->B = false;
        Mask->A = false;
        Mask->MaterialExpressionEditorX = -400;
        Mask->MaterialExpressionEditorY = 800 + i * 200;
        Mask->Input.Connect(0, Sampler);
        Exprs.AddExpression(Mask);
        AlphaMasks[i] = Mask;
    }

    // --- Lerp chain: blend layers using alpha maps ---
    // Lerp1 = lerp(Base, Layer1, Alpha1)
    auto* Lerp1 = NewObject<UMaterialExpressionLinearInterpolate>(CachedMat);
    Lerp1->MaterialExpressionEditorX = -200;
    Lerp1->MaterialExpressionEditorY = 0;
    Lerp1->A.Connect(0, GroundSamplers[0]); // Base RGB
    Lerp1->B.Connect(0, GroundSamplers[1]); // Layer1 RGB
    Lerp1->Alpha.Connect(0, AlphaMasks[0]); // Alpha1 R
    Exprs.AddExpression(Lerp1);

    // Lerp2 = lerp(Lerp1, Layer2, Alpha2)
    auto* Lerp2 = NewObject<UMaterialExpressionLinearInterpolate>(CachedMat);
    Lerp2->MaterialExpressionEditorX = -0;
    Lerp2->MaterialExpressionEditorY = 100;
    Lerp2->A.Connect(0, Lerp1);
    Lerp2->B.Connect(0, GroundSamplers[2]); // Layer2 RGB
    Lerp2->Alpha.Connect(0, AlphaMasks[1]); // Alpha2 R
    Exprs.AddExpression(Lerp2);

    // Lerp3 = lerp(Lerp2, Layer3, Alpha3)
    auto* Lerp3 = NewObject<UMaterialExpressionLinearInterpolate>(CachedMat);
    Lerp3->MaterialExpressionEditorX = 200;
    Lerp3->MaterialExpressionEditorY = 200;
    Lerp3->A.Connect(0, Lerp2);
    Lerp3->B.Connect(0, GroundSamplers[3]); // Layer3 RGB
    Lerp3->Alpha.Connect(0, AlphaMasks[2]); // Alpha3 R
    Exprs.AddExpression(Lerp3);

    // --- Connect final lerp output to BaseColor ---
    CachedMat->GetEditorOnlyData()->BaseColor.Connect(0, Lerp3);

    // --- Runtime Virtual Texture output ---
    // This allows the terrain to render into an RVT for LOD/caching
    auto* RVTOutput = NewObject<UMaterialExpressionRuntimeVirtualTextureOutput>(CachedMat);
    RVTOutput->MaterialExpressionEditorX = 400;
    RVTOutput->MaterialExpressionEditorY = 0;
    RVTOutput->BaseColor.Connect(0, Lerp3); // BaseColor output to RVT
    Exprs.AddExpression(RVTOutput);

    // Compile the material
    CachedMat->PreEditChange(nullptr);
    CachedMat->PostEditChange();

    UE_LOG(LogTerrainMat, Log, TEXT("4-layer terrain splat material created and compiled"));
#else
    // Non-editor builds: fall back to BasicShapeMaterial
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
