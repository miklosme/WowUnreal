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
#include "UObject/UObjectGlobals.h"
#include "Misc/PackageName.h"
#include "Misc/App.h"
#include "UObject/SavePackage.h"
#if WITH_EDITOR
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
#include "ShaderCompiler.h"
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

    // Debug: log first chunk's texture info
    static bool bLoggedFirst = false;
    if (!bLoggedFirst)
    {
        bLoggedFirst = true;
        UE_LOG(LogTerrainMat, Log, TEXT("First chunk [%d,%d]: %d layer textures, %d alpha maps, base tex=%s (%dx%d)"),
            ChunkData.IndexX, ChunkData.IndexY,
            LayerTextures.Num(), ChunkData.AlphaMaps.Num(),
            LayerTextures[0] ? *LayerTextures[0]->GetName() : TEXT("null"),
            LayerTextures[0] ? LayerTextures[0]->GetSizeX() : 0,
            LayerTextures[0] ? LayerTextures[0]->GetSizeY() : 0);
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

    // Debug: log material info once
    static bool bLoggedMat = false;
    if (!bLoggedMat)
    {
        bLoggedMat = true;
        UE_LOG(LogTerrainMat, Log, TEXT("MID parent: %s, IsComplete=%d"),
            *BaseMat->GetName(), BaseMat->IsComplete() ? 1 : 0);
    }

    // Set the base layer texture (always present)
    MID->SetTextureParameterValue(FName(TEXT("Layer0Texture")), LayerTextures[0]);
    UE_LOG(LogTerrainMat, Verbose, TEXT("Chunk [%d,%d]: assigned Layer0Texture %s"),
        ChunkData.IndexX, ChunkData.IndexY, *LayerTextures[0]->GetName());

    // Set additional layer textures and their alpha maps
    static const FName LayerParamNames[] = {
        FName(TEXT("Layer1Texture")),
        FName(TEXT("Layer2Texture")),
        FName(TEXT("Layer3Texture"))
    };
    static const FName AlphaParamNames[] = {
        FName(TEXT("AlphaMap1")),
        FName(TEXT("AlphaMap2")),
        FName(TEXT("AlphaMap3"))
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

    // Try loading a pre-compiled material asset (created by Scripts/CreateTerrainPreviewMaterial.py)
    const TCHAR* MatPaths[] = {
        TEXT("/Game/Wow/Materials/M_WowTerrainPreview"),
        TEXT("/Game/Materials/M_WowTerrain"),
        TEXT("/Game/Materials/M_WowTerrainRuntime")
    };
    for (const TCHAR* Path : MatPaths)
    {
        FSoftObjectPath MatPath(FString(Path) + TEXT(".") + FPaths::GetBaseFilename(Path));
        if (MatPath.ResolveObject() || FPackageName::DoesPackageExist(MatPath.GetLongPackageName()))
        {
            CachedMat = LoadObject<UMaterial>(nullptr, Path);
            if (CachedMat)
            {
                UE_LOG(LogTerrainMat, Log, TEXT("Loaded terrain material from %s"), Path);
#if WITH_EDITOR
                // Ensure shaders are compiled before using — on first run the Metal
                // shaders may not be cached yet, causing grey rendering.
                if (!CachedMat->IsComplete())
                {
                    UE_LOG(LogTerrainMat, Log, TEXT("Material shaders not compiled yet, compiling..."));
                    CachedMat->ForceRecompileForRendering();
                    if (GShaderCompilingManager)
                    {
                        GShaderCompilingManager->FinishAllCompilation();
                    }
                    UE_LOG(LogTerrainMat, Log, TEXT("Material shader compilation done, IsComplete=%d"),
                        CachedMat->IsComplete() ? 1 : 0);
                }
#endif
                return CachedMat;
            }
        }
    }

#if WITH_EDITOR
    // Build the full 4-layer terrain splat material graph programmatically.
    // Layout:
    //   UV0 (0..1 per chunk) → alpha map samplers
    //   UV1 (world-space tiled) → ground texture samplers
    //   Lerp chain: Base → Layer1 (Alpha1) → Layer2 (Alpha2) → Layer3 (Alpha3) → BaseColor
    UE_LOG(LogTerrainMat, Log, TEXT("Creating 4-layer terrain splat material"));

    UPackage* MatPackage = CreatePackage(TEXT("/Game/Materials/M_WowTerrainRuntime"));
    CachedMat = NewObject<UMaterial>(MatPackage, TEXT("M_WowTerrainRuntime"),
        RF_Public | RF_Standalone);
    CachedMat->SetShadingModel(MSM_DefaultLit);
    CachedMat->TwoSided = false;

    UTexture2D* WhiteTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture"));
    UTexture2D* BlackTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/Black"));

    auto& Exprs = CachedMat->GetExpressionCollection();

    // --- Texture coordinate nodes ---
    // UV1 = world-space tiled UVs for ground texture sampling
    auto* TexCoordTiled = NewObject<UMaterialExpressionTextureCoordinate>(CachedMat);
    TexCoordTiled->CoordinateIndex = 1;
    TexCoordTiled->MaterialExpressionEditorX = -800;
    TexCoordTiled->MaterialExpressionEditorY = 0;
    Exprs.AddExpression(TexCoordTiled);

    // UV0 = per-chunk [0,1] for alpha/splatmap lookup
    auto* TexCoordAlpha = NewObject<UMaterialExpressionTextureCoordinate>(CachedMat);
    TexCoordAlpha->CoordinateIndex = 0;
    TexCoordAlpha->MaterialExpressionEditorX = -800;
    TexCoordAlpha->MaterialExpressionEditorY = 800;
    Exprs.AddExpression(TexCoordAlpha);

    // --- 4 ground texture samplers (use UV1 for world-space tiling) ---
    const TCHAR* GroundParamNames[] = {
        TEXT("Layer0Texture"), TEXT("Layer1Texture"), TEXT("Layer2Texture"), TEXT("Layer3Texture")
    };
    UMaterialExpressionTextureSampleParameter2D* GroundSamplers[4];

    for (int32 i = 0; i < 4; ++i)
    {
        auto* Sampler = NewObject<UMaterialExpressionTextureSampleParameter2D>(CachedMat);
        Sampler->ParameterName = FName(GroundParamNames[i]);
        Sampler->SamplerType = SAMPLERTYPE_Color;
        Sampler->Texture = (i == 0) ? WhiteTex : BlackTex;
        Sampler->Coordinates.Connect(0, TexCoordTiled);
        Sampler->MaterialExpressionEditorX = -600;
        Sampler->MaterialExpressionEditorY = i * 200;
        Exprs.AddExpression(Sampler);
        GroundSamplers[i] = Sampler;
    }

    // --- 3 alpha map samplers (use UV0 for per-chunk splatmap) ---
    const TCHAR* AlphaParamNamesArr[] = {
        TEXT("AlphaMap1"), TEXT("AlphaMap2"), TEXT("AlphaMap3")
    };
    UMaterialExpressionTextureSampleParameter2D* AlphaSamplers[3];
    UMaterialExpressionComponentMask* AlphaMasks[3];

    for (int32 i = 0; i < 3; ++i)
    {
        auto* Sampler = NewObject<UMaterialExpressionTextureSampleParameter2D>(CachedMat);
        Sampler->ParameterName = FName(AlphaParamNamesArr[i]);
        Sampler->SamplerType = SAMPLERTYPE_Color;
        Sampler->Texture = BlackTex;
        Sampler->Coordinates.Connect(0, TexCoordAlpha);
        Sampler->MaterialExpressionEditorX = -600;
        Sampler->MaterialExpressionEditorY = 800 + i * 200;
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

    // --- Connect final lerp output to BaseColor + roughness/specular ---
    CachedMat->GetEditorOnlyData()->BaseColor.Connect(0, Lerp3);

    auto* RoughnessConst = NewObject<UMaterialExpressionConstant>(CachedMat);
    RoughnessConst->R = 0.85f; // Slightly less than full rough for subtle sheen on terrain
    RoughnessConst->MaterialExpressionEditorX = 200;
    RoughnessConst->MaterialExpressionEditorY = 400;
    Exprs.AddExpression(RoughnessConst);
    CachedMat->GetEditorOnlyData()->Roughness.Connect(0, RoughnessConst);

    auto* SpecularConst = NewObject<UMaterialExpressionConstant>(CachedMat);
    SpecularConst->R = 0.15f; // Small amount of specular for natural terrain
    SpecularConst->MaterialExpressionEditorX = 200;
    SpecularConst->MaterialExpressionEditorY = 500;
    Exprs.AddExpression(SpecularConst);
    CachedMat->GetEditorOnlyData()->Specular.Connect(0, SpecularConst);

    // Compile the material — must be in a real package for Metal shader compilation
    CachedMat->PreEditChange(nullptr);
    CachedMat->PostEditChange();
    CachedMat->ForceRecompileForRendering();

    // Block until shader compilation is done — otherwise MIDs will render grey
    if (GShaderCompilingManager)
    {
        UE_LOG(LogTerrainMat, Log, TEXT("Waiting for terrain material shader compilation..."));
        GShaderCompilingManager->FinishAllCompilation();
        UE_LOG(LogTerrainMat, Log, TEXT("Shader compilation complete, IsComplete=%d"), CachedMat->IsComplete() ? 1 : 0);
    }

    // Save the material to disk so it persists for subsequent launches
    {
        FString PackageFilename = FPackageName::LongPackageNameToFilename(
            TEXT("/Game/Materials/M_WowTerrainRuntime"), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        UPackage::SavePackage(MatPackage, CachedMat, *PackageFilename, SaveArgs);
        UE_LOG(LogTerrainMat, Log, TEXT("Saved terrain material to: %s"), *PackageFilename);
    }

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

    // Alpha maps are generated per chunk at runtime, so they need unique transient names
    // to avoid replacing textures that are still referenced by already-loaded tiles.
    const FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), *Name);
    UTexture2D* Tex = UTexture2D::CreateTransient(AlphaSize, AlphaSize, PF_G8, UniqueName);
    if (!Tex) return nullptr;

    Tex->Filter = TF_Bilinear;
    Tex->SRGB = false; // Alpha data is linear
    Tex->NeverStream = true;
    Tex->LODGroup = TEXTUREGROUP_Terrain_Weightmap;
    Tex->AddressX = TA_Clamp;
    Tex->AddressY = TA_Clamp;

    // ADT parser reads alpha data in row-major order (row=south, col=east),
    // which directly matches UE's texture layout and our UV0 mapping.
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

UMaterial* FWowTerrainMaterial::GetSimpleObjectMaterial()
{
    static UMaterial* CachedMat = nullptr;
    if (CachedMat && CachedMat->IsValidLowLevel()) return CachedMat;

    // Try loading pre-compiled version first
    const TCHAR* MatPaths[] = {
        TEXT("/Game/Wow/Materials/M_WowSimpleObject"),
        TEXT("/Game/Materials/M_WowSimpleObject")
    };
    for (const TCHAR* Path : MatPaths)
    {
        FSoftObjectPath MatPath(FString(Path) + TEXT(".") + FPaths::GetBaseFilename(Path));
        if (MatPath.ResolveObject() || FPackageName::DoesPackageExist(MatPath.GetLongPackageName()))
        {
            CachedMat = LoadObject<UMaterial>(nullptr, Path);
            if (CachedMat)
            {
                UE_LOG(LogTerrainMat, Log, TEXT("Loaded simple object material from %s"), Path);
#if WITH_EDITOR
                if (!CachedMat->IsComplete())
                {
                    CachedMat->ForceRecompileForRendering();
                    if (GShaderCompilingManager)
                        GShaderCompilingManager->FinishAllCompilation();
                }
#endif
                return CachedMat;
            }
        }
    }

#if WITH_EDITOR
    // Build a simple single-texture material: samples UV0 with one texture parameter
    UE_LOG(LogTerrainMat, Log, TEXT("Creating simple object material (UV0-based)"));

    UPackage* MatPackage = CreatePackage(TEXT("/Game/Materials/M_WowSimpleObject"));
    CachedMat = NewObject<UMaterial>(MatPackage, TEXT("M_WowSimpleObject"),
        RF_Public | RF_Standalone);
    CachedMat->SetShadingModel(MSM_DefaultLit);
    CachedMat->BlendMode = BLEND_Masked; // Alpha masking for foliage/transparent textures
    CachedMat->TwoSided = true; // Foliage needs two-sided rendering
    CachedMat->bUsedWithInstancedStaticMeshes = true; // Required for HISMC/ISM rendering

    UTexture2D* WhiteTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture"));

    auto& Exprs = CachedMat->GetExpressionCollection();

    // UV0 for texture sampling (standard object UVs)
    auto* TexCoord = NewObject<UMaterialExpressionTextureCoordinate>(CachedMat);
    TexCoord->CoordinateIndex = 0;
    TexCoord->MaterialExpressionEditorX = -400;
    TexCoord->MaterialExpressionEditorY = 0;
    Exprs.AddExpression(TexCoord);

    // Single texture parameter
    auto* Sampler = NewObject<UMaterialExpressionTextureSampleParameter2D>(CachedMat);
    Sampler->ParameterName = FName(TEXT("Layer0Texture"));
    Sampler->SamplerType = SAMPLERTYPE_Color;
    Sampler->Texture = WhiteTex;
    Sampler->Coordinates.Connect(0, TexCoord);
    Sampler->MaterialExpressionEditorX = -200;
    Sampler->MaterialExpressionEditorY = 0;
    Exprs.AddExpression(Sampler);

    CachedMat->GetEditorOnlyData()->BaseColor.Connect(0, Sampler);
    CachedMat->GetEditorOnlyData()->OpacityMask.Connect(4, Sampler); // Output 4 = Alpha channel

    auto* RoughnessConst = NewObject<UMaterialExpressionConstant>(CachedMat);
    RoughnessConst->R = 0.8f;
    RoughnessConst->MaterialExpressionEditorX = 0;
    RoughnessConst->MaterialExpressionEditorY = 200;
    Exprs.AddExpression(RoughnessConst);
    CachedMat->GetEditorOnlyData()->Roughness.Connect(0, RoughnessConst);

    CachedMat->PreEditChange(nullptr);
    CachedMat->PostEditChange();
    CachedMat->ForceRecompileForRendering();

    if (GShaderCompilingManager)
    {
        GShaderCompilingManager->FinishAllCompilation();
        UE_LOG(LogTerrainMat, Log, TEXT("Simple object material compiled, IsComplete=%d"),
            CachedMat->IsComplete() ? 1 : 0);
    }

    // Save to disk
    {
        FString PackageFilename = FPackageName::LongPackageNameToFilename(
            TEXT("/Game/Materials/M_WowSimpleObject"), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        UPackage::SavePackage(MatPackage, CachedMat, *PackageFilename, SaveArgs);
    }
#else
    CachedMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
#endif

    return CachedMat;
}
