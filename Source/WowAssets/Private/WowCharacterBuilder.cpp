#include "WowCharacterBuilder.h"
#include "WowSkeletalMeshBuilder.h"
#include "WowCharacterTexture.h"
#include "WowEquipmentManager.h"
#include "WowAssetCache.h"
#include "WowTextureFactory.h"
#include "Mpq/MpqManager.h"
#include "Formats/M2Parser.h"
#include "Formats/M2Types.h"
#include "Formats/BlpParser.h"
#include "Formats/BlpTypes.h"
#include "Formats/Dbc/DbcStore.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Coord/WowCoordinate.h"
#include "UObject/SoftObjectPath.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#if WITH_EDITOR
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionConstant.h"
#include "UObject/UObjectGlobals.h"
#include "ShaderCompiler.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWowCharacter, Log, All);

// Parsed M2 cache (shared with doodad manager via static)
static TMap<FString, TSharedPtr<FM2Data>> CharM2Cache;
static FCriticalSection CharCacheLock;
static TMap<FString, TWeakObjectPtr<USkeleton>> CharSkeletonCache;
static TMap<FString, TArray<TWeakObjectPtr<UAnimSequence>>> CharAnimCache;
static TMap<FString, TArray<FGeosetSectionInfo>> CharGeosetCache;

namespace
{
UMaterial* GetCharacterMaterial()
{
    static UMaterial* CachedMat = nullptr;
    if (CachedMat && CachedMat->IsValidLowLevel()) return CachedMat;

    // Try loading a pre-compiled version first
    const TCHAR* MatPaths[] = {
        TEXT("/Game/Wow/Materials/M_WowCharacter"),
        TEXT("/Game/Materials/M_WowCharacter")
    };
    for (const TCHAR* Path : MatPaths)
    {
        FSoftObjectPath MatPath(FString(Path) + TEXT(".") + FPaths::GetBaseFilename(Path));
        if (MatPath.ResolveObject() || FPackageName::DoesPackageExist(MatPath.GetLongPackageName()))
        {
            CachedMat = LoadObject<UMaterial>(nullptr, Path);
            if (CachedMat)
            {
                UE_LOG(LogWowCharacter, Log, TEXT("Loaded character material from %s"), Path);
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
    UE_LOG(LogWowCharacter, Log, TEXT("Creating character material (UV0-based, DefaultLit with PBR)"));

    UPackage* MatPackage = CreatePackage(TEXT("/Game/Materials/M_WowCharacter"));
    CachedMat = NewObject<UMaterial>(MatPackage, TEXT("M_WowCharacter"),
        RF_Public | RF_Standalone);
    CachedMat->SetShadingModel(MSM_DefaultLit);
    CachedMat->BlendMode = BLEND_Masked; // Alpha cutout for hair transparency
    CachedMat->TwoSided = true;          // Hair and some body parts need two-sided
    CachedMat->bUsedWithSkeletalMesh = true;

    UTexture2D* WhiteTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture"));
    auto& Expressions = CachedMat->GetExpressionCollection();

    // UV0 for texture sampling
    auto* TexCoord = NewObject<UMaterialExpressionTextureCoordinate>(CachedMat);
    TexCoord->CoordinateIndex = 0;
    TexCoord->MaterialExpressionEditorX = -400;
    TexCoord->MaterialExpressionEditorY = 0;
    Expressions.AddExpression(TexCoord);

    auto* TextureSampler = NewObject<UMaterialExpressionTextureSampleParameter2D>(CachedMat);
    TextureSampler->ParameterName = TEXT("BaseTexture");
    TextureSampler->SamplerType = SAMPLERTYPE_Color;
    TextureSampler->Texture = WhiteTex;
    TextureSampler->Coordinates.Connect(0, TexCoord);
    TextureSampler->MaterialExpressionEditorX = -200;
    TextureSampler->MaterialExpressionEditorY = 0;
    Expressions.AddExpression(TextureSampler);

    // DefaultLit: connect texture to BaseColor and alpha to OpacityMask
    CachedMat->GetEditorOnlyData()->BaseColor.Connect(0, TextureSampler);
    CachedMat->GetEditorOnlyData()->OpacityMask.Connect(4, TextureSampler); // Output 4 = Alpha

    // Add Roughness constant (1.0 = fully rough, no specular highlights)
    auto* RoughnessConstant = NewObject<UMaterialExpressionConstant>(CachedMat);
    RoughnessConstant->R = 1.0f;
    RoughnessConstant->MaterialExpressionEditorX = -200;
    RoughnessConstant->MaterialExpressionEditorY = 100;
    Expressions.AddExpression(RoughnessConstant);
    CachedMat->GetEditorOnlyData()->Roughness.Connect(0, RoughnessConstant);

    // Add Metallic constant
    auto* MetallicConstant = NewObject<UMaterialExpressionConstant>(CachedMat);
    MetallicConstant->R = 0.0f;
    MetallicConstant->MaterialExpressionEditorX = -200;
    MetallicConstant->MaterialExpressionEditorY = 200;
    Expressions.AddExpression(MetallicConstant);
    CachedMat->GetEditorOnlyData()->Metallic.Connect(0, MetallicConstant);

    // Add Specular constant (0.0 = no specular, WoW-style diffuse only)
    auto* SpecularConstant = NewObject<UMaterialExpressionConstant>(CachedMat);
    SpecularConstant->R = 0.0f;
    SpecularConstant->MaterialExpressionEditorX = -200;
    SpecularConstant->MaterialExpressionEditorY = 300;
    Expressions.AddExpression(SpecularConstant);
    CachedMat->GetEditorOnlyData()->Specular.Connect(0, SpecularConstant);

    CachedMat->PreEditChange(nullptr);
    CachedMat->PostEditChange();
    CachedMat->ForceRecompileForRendering();

    // Block until shader compilation is done — otherwise skeletal meshes render black
    if (GShaderCompilingManager)
    {
        UE_LOG(LogWowCharacter, Log, TEXT("Waiting for character material shader compilation..."));
        GShaderCompilingManager->FinishAllCompilation();
        UE_LOG(LogWowCharacter, Log, TEXT("Character material compiled, IsComplete=%d"),
            CachedMat->IsComplete() ? 1 : 0);
    }

    // Save to disk so it persists for subsequent launches
    {
        FString PackageFilename = FPackageName::LongPackageNameToFilename(
            TEXT("/Game/Materials/M_WowCharacter"), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        UPackage::SavePackage(MatPackage, CachedMat, *PackageFilename, SaveArgs);
        UE_LOG(LogWowCharacter, Log, TEXT("Saved character material to: %s"), *PackageFilename);
    }
#endif

    if (!CachedMat)
    {
        CachedMat = UMaterial::GetDefaultMaterial(MD_Surface);
    }
    return CachedMat;
}

UTexture2D* LoadBlpTexture(FMpqManager* Mpq, FWowAssetCache* Cache, const FString& TexturePath)
{
    if (!Mpq || TexturePath.IsEmpty())
    {
        return nullptr;
    }

    if (Cache)
    {
        if (UTexture2D* Cached = Cache->FindTexture(TexturePath))
        {
            return Cached;
        }
    }

    TArray<uint8> BlpRaw;
    if (!Mpq->ReadFile(TexturePath, BlpRaw))
    {
        return nullptr;
    }

    FBlpTexture BlpData = FBlpParser::Parse(BlpRaw);
    if (!BlpData.bIsValid)
    {
        return nullptr;
    }

    UTexture2D* Texture = FWowTextureFactory::CreateTexture(BlpData, TexturePath);
    if (Texture && Cache)
    {
        Cache->CacheTexture(TexturePath, Texture);
    }
    return Texture;
}

void ApplyTexturedMaterial(UPrimitiveComponent* Component, UTexture2D* Texture)
{
    if (!Component || !Texture)
    {
        return;
    }

    if (UMaterial* BaseMat = GetCharacterMaterial())
    {
        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, Component);
        MID->SetTextureParameterValue(TEXT("BaseTexture"), Texture);
        Component->SetMaterial(0, MID);
    }
}

int32 FindPreferredAnimationIndex(const FM2Data& Data)
{
    int32 FirstLooping = INDEX_NONE;
    for (int32 Index = 0; Index < Data.AnimationTracks.Num(); ++Index)
    {
        const FM2AnimationData& Track = Data.AnimationTracks[Index];
        if (Track.AnimationId == 0)
        {
            return Index;
        }

        if (FirstLooping == INDEX_NONE && Track.bIsLooping)
        {
            FirstLooping = Index;
        }
    }

    return FirstLooping != INDEX_NONE ? FirstLooping : 0;
}

UAnimSequence* CreateReferencePoseAnimation(USkeleton* Skeleton, const FString& ModelName)
{
    if (!Skeleton)
    {
        return nullptr;
    }

    UAnimSequence* AnimSeq = NewObject<UAnimSequence>();
    AnimSeq->SetSkeleton(Skeleton);

    IAnimationDataController& Controller = AnimSeq->GetController();
    Controller.InitializeModel();
    Controller.OpenBracket(FText::FromString(TEXT("CreateReferencePoseAnimation")));
    Controller.SetFrameRate(FFrameRate(30, 1));
    Controller.SetNumberOfFrames(1);

    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
    for (int32 BoneIndex = 0; BoneIndex < RefSkeleton.GetNum(); ++BoneIndex)
    {
        const FName BoneName = RefSkeleton.GetBoneName(BoneIndex);
        const FTransform& BoneTransform = RefPose[BoneIndex];

        Controller.AddBoneCurve(BoneName);
        Controller.SetBoneTrackKeys(
            BoneName,
            { FVector3f(BoneTransform.GetTranslation()) },
            { FQuat4f(BoneTransform.GetRotation()) },
            { FVector3f(BoneTransform.GetScale3D()) });
    }

    Controller.CloseBracket();
    Controller.NotifyPopulated();

    UE_LOG(LogWowCharacter, Log, TEXT("Created reference-pose animation for %s"), *ModelName);
    return AnimSeq;
}

UTexture2D* ResolveCreatureTextureOverride(FMpqManager* Mpq, FWowAssetCache* Cache,
    const FString& ModelPath, const FString& TexturePath)
{
    if (TexturePath.IsEmpty())
    {
        return nullptr;
    }

    FString ResolvedPath = TexturePath;
    ResolvedPath.ReplaceInline(TEXT("/"), TEXT("\\"));
    if (!ResolvedPath.EndsWith(TEXT(".blp"), ESearchCase::IgnoreCase))
    {
        ResolvedPath += TEXT(".blp");
    }

    if (UTexture2D* Texture = LoadBlpTexture(Mpq, Cache, ResolvedPath))
    {
        return Texture;
    }

    FString RelativePath = FPaths::Combine(FPaths::GetPath(ModelPath), FPaths::GetCleanFilename(ResolvedPath));
    RelativePath.ReplaceInline(TEXT("/"), TEXT("\\"));
    return LoadBlpTexture(Mpq, Cache, RelativePath);
}
}

TMap<uint16, uint16> FWowCharacterBuilder::ComputeDefaultGeosets(const FWowCharacterTexture::FCustomization& Customization)
{
    TMap<uint16, uint16> Result;
    const FDbcStore& Dbc = FDbcStore::Get();

    // WMV defaults ALL groups to variant 1. Variant 0 = hide entire group.
    // GeosetId = group*100 + variant. Group 0 variant 1 = geosetId 1 (body mesh).

    // Group 0 (CG_SKIN_OR_HAIRSTYLE): Hair mesh is in group 0!
    // GeosetId=0 (base body) always visible via special case in mesh filter.
    // Hair geoset ID from CharHairGeosets sets which additional group 0 variant to show.
    {
        const FCharHairGeosetsDbcEntry* HairEntry = Dbc.CharHairGeosets().GetByRaceGenderVariation(
            Customization.RaceId, Customization.Gender, Customization.HairStyle);

        uint16 HairVariant = 1; // Default: variant 1 = basic body
        if (HairEntry)
        {
            HairVariant = static_cast<uint16>(FMath::Max(1u, HairEntry->GeosetID));
            UE_LOG(LogWowCharacter, Log, TEXT("Hair geoset: Race=%d Gender=%d Style=%d → group 0 variant %d"),
                Customization.RaceId, Customization.Gender, Customization.HairStyle, HairVariant);
        }
        Result.Add(0, HairVariant);
    }

    // Groups 1, 2, 3 (Facial features): From CharacterFacialHairStyles DBC
    // Group 1 = Geosets[0] (tusks/chin), Group 2 = Geosets[1], Group 3 = Geosets[2]
    {
        const FCharacterFacialHairStylesDbcEntry* FacialEntry = Dbc.CharacterFacialHairStyles().GetByRaceGenderVariation(
            Customization.RaceId, Customization.Gender, Customization.FacialHairStyle);

        if (FacialEntry)
        {
            Result.Add(1, static_cast<uint16>(FacialEntry->Geosets[0] > 0 ? FacialEntry->Geosets[0] : 1));
            Result.Add(2, static_cast<uint16>(FacialEntry->Geosets[1] > 0 ? FacialEntry->Geosets[1] : 1));
            Result.Add(3, static_cast<uint16>(FacialEntry->Geosets[2] > 0 ? FacialEntry->Geosets[2] : 1));
        }
        else
        {
            Result.Add(1, 1);
            Result.Add(2, 1);
            Result.Add(3, 1);
        }
    }
    Result.Add(4, 1);  // Forearms/gloves: variant 1 = bare forearms
    Result.Add(5, 1);  // Shins/boots: variant 1 = bare shins
    Result.Add(6, 1);  // Tail/misc
    Result.Add(7, 2);  // Ears: visible (WMV uses 2)
    Result.Add(8, 1);  // Wristbands/sleeves
    Result.Add(9, 1);  // Kneepads
    Result.Add(10, 1); // Chest
    Result.Add(11, 1); // Pants lower
    Result.Add(12, 1); // Tabard
    Result.Add(13, 1); // Trousers
    Result.Add(14, 1); // Loincloth
    Result.Add(15, 1); // Cape

    for (uint16 g = 16; g <= 18; ++g)
        Result.Add(g, 1);

    return Result;
}

void FWowCharacterBuilder::ApplyGeosetVisibility(USkeletalMeshComponent* MeshComp,
    const TArray<FGeosetSectionInfo>& GeosetInfo,
    const TMap<uint16, uint16>& VisibleGeosets)
{
    if (!MeshComp || GeosetInfo.Num() == 0)
    {
        return;
    }

    int32 VisibleCount = 0;
    int32 HiddenCount = 0;

    for (const FGeosetSectionInfo& Info : GeosetInfo)
    {
        bool bShouldBeVisible = true; // Default: show unless explicitly filtered

        if (Info.GeosetId == 0)
        {
            // GeosetId 0 = base body mesh in 3.3.5 M2, always visible
            bShouldBeVisible = true;
        }
        else
        {
            const uint16* DesiredVariant = VisibleGeosets.Find(Info.GeosetGroup);
            if (DesiredVariant)
            {
                // For groups with a rule, show only the matching variant
                bShouldBeVisible = (Info.GeosetVariant == *DesiredVariant);
            }
            // else: Groups not in our rule set: show by default (creatures, etc.)
        }

        // MaterialID = SectionIndex since we have one material slot per section.
        // SectionIndex param = INDEX_NONE means use MaterialID directly.
        MeshComp->ShowMaterialSection(Info.SectionIndex, Info.SectionIndex, bShouldBeVisible, 0);

        if (bShouldBeVisible)
        {
            VisibleCount++;
        }
        else
        {
            HiddenCount++;
        }
    }

    UE_LOG(LogWowCharacter, Log, TEXT("Applied geoset visibility: %d visible, %d hidden (of %d sections)"),
        VisibleCount, HiddenCount, GeosetInfo.Num());
}

FString FWowCharacterBuilder::GetCharacterModelPath(ERace Race, EGender Gender)
{
    const FDbcStore& Dbc = FDbcStore::Get();
    uint32 RaceId = static_cast<uint32>(Race);
    const FChrRacesDbcEntry* RaceEntry = Dbc.ChrRaces().GetById(RaceId);

    if (!RaceEntry)
    {
        UE_LOG(LogWowCharacter, Warning, TEXT("Unknown race ID %d"), RaceId);
        return FString();
    }

    FString RaceStr = RaceEntry->ClientFileString;
    FString GenderStr = (Gender == EGender::Male) ? TEXT("Male") : TEXT("Female");

    return FString::Printf(TEXT("Character\\%s\\%s\\%s%s.m2"), *RaceStr, *GenderStr, *RaceStr, *GenderStr);
}

AActor* FWowCharacterBuilder::SpawnCharacter(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
    ERace Race, EGender Gender, const FVector& Location, const FRotator& Rotation)
{
    FString ModelPath = GetCharacterModelPath(Race, Gender);
    if (ModelPath.IsEmpty()) return nullptr;

    UE_LOG(LogWowCharacter, Log, TEXT("Spawning character: Race=%d Gender=%d Model=%s"),
        static_cast<int32>(Race), static_cast<int32>(Gender), *ModelPath);

    // Build composite texture with default customization
    FWowCharacterTexture::FCustomization Cust;
    Cust.RaceId = static_cast<uint32>(Race);
    Cust.Gender = static_cast<uint32>(Gender);
    UTexture2D* CompositeTex = FWowCharacterTexture::BuildCompositeTexture(Mpq, Cache, Cust);

    return SpawnM2Actor(World, Mpq, Cache, ModelPath, Location, Rotation, 1.0f, CompositeTex, &Cust);
}

AActor* FWowCharacterBuilder::SpawnCharacterWithEquipment(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
    const FCharacterParams& Params, const FVector& Location, const FRotator& Rotation)
{
    FString ModelPath = GetCharacterModelPath(Params.Race, Params.Gender);
    if (ModelPath.IsEmpty()) return nullptr;

    UE_LOG(LogWowCharacter, Log, TEXT("Spawning equipped character: Race=%d Gender=%d Equipment=%d slots"),
        static_cast<int32>(Params.Race), static_cast<int32>(Params.Gender), Params.Equipment.Num());

    // Build composite texture with full customization
    UTexture2D* CompositeTex = FWowCharacterTexture::BuildCompositeTexture(Mpq, Cache, Params.Customization);

    // Spawn base character with geoset visibility
    AActor* Actor = SpawnM2Actor(World, Mpq, Cache, ModelPath, Location, Rotation, 1.0f, CompositeTex, &Params.Customization);
    if (!Actor) return nullptr;

    // Attach equipment
    if (Params.Equipment.Num() > 0)
    {
        USkeletalMeshComponent* MeshComp = Actor->FindComponentByClass<USkeletalMeshComponent>();
        if (MeshComp)
        {
            // Get the character's M2 data for attachment point bone lookup
            FString NormPath = ModelPath.ToLower();
            NormPath.ReplaceInline(TEXT("/"), TEXT("\\"));

            TSharedPtr<FM2Data> CharM2;
            {
                FScopeLock Lock(&CharCacheLock);
                TSharedPtr<FM2Data>* Found = CharM2Cache.Find(NormPath);
                if (Found) CharM2 = *Found;
            }

            if (CharM2)
            {
                for (const FEquipmentSlot& Slot : Params.Equipment)
                {
                    if (Slot.ItemDisplayId == 0) continue;

                    USceneComponent* EquipComp = FWowEquipmentManager::AttachEquipment(
                        Mpq, Cache, Slot.ItemDisplayId, Slot.AttachPoint, MeshComp, *CharM2);

                    if (EquipComp)
                    {
                        UE_LOG(LogWowCharacter, Log, TEXT("Equipped item DisplayID=%d at attachment %d"),
                            Slot.ItemDisplayId, static_cast<uint32>(Slot.AttachPoint));
                    }
                }
            }
            else
            {
                UE_LOG(LogWowCharacter, Warning, TEXT("No cached M2 data for equipment attachment: %s"), *NormPath);
            }
        }
    }

    return Actor;
}

AActor* FWowCharacterBuilder::SpawnCreatureByDisplayId(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
    uint32 DisplayId, const FVector& Location, const FRotator& Rotation)
{
    const FDbcStore& Dbc = FDbcStore::Get();
    const FCreatureDisplayInfoDbcEntry* DisplayInfo = Dbc.CreatureDisplayInfo().GetById(DisplayId);
    if (!DisplayInfo)
    {
        UE_LOG(LogWowCharacter, Warning, TEXT("Unknown CreatureDisplayInfo ID %d"), DisplayId);
        return nullptr;
    }

    const FCreatureModelDataDbcEntry* ModelData = Dbc.CreatureModelData().GetById(DisplayInfo->ModelID);
    if (!ModelData)
    {
        UE_LOG(LogWowCharacter, Warning, TEXT("Unknown CreatureModelData ID %d for display %d"),
            DisplayInfo->ModelID, DisplayId);
        return nullptr;
    }

    FString ModelPath = ModelData->ModelPath;
    if (ModelPath.IsEmpty()) return nullptr;

    if (ModelPath.EndsWith(TEXT(".mdx"), ESearchCase::IgnoreCase))
    {
        ModelPath = ModelPath.Left(ModelPath.Len() - 4) + TEXT(".m2");
    }

    UTexture2D* TextureOverride = ResolveCreatureTextureOverride(Mpq, Cache, ModelPath, DisplayInfo->Texture1);

    float Scale = DisplayInfo->Scale * ModelData->Scale;
    UE_LOG(LogWowCharacter, Log, TEXT("Spawning creature: DisplayID=%d Model=%s Scale=%.2f"),
        DisplayId, *ModelPath, Scale);

    return SpawnM2Actor(World, Mpq, Cache, ModelPath, Location, Rotation, Scale, TextureOverride);
}

AActor* FWowCharacterBuilder::SpawnM2Actor(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
    const FString& ModelPath, const FVector& Location, const FRotator& Rotation, float Scale,
    UTexture2D* OverrideTexture, const FWowCharacterTexture::FCustomization* Customization)
{
    if (!World || !Mpq) return nullptr;

    FString NormPath = ModelPath.ToLower();
    NormPath.ReplaceInline(TEXT("/"), TEXT("\\"));

    // Parse M2
    TSharedPtr<FM2Data> M2Data;
    {
        FScopeLock Lock(&CharCacheLock);
        TSharedPtr<FM2Data>* Found = CharM2Cache.Find(NormPath);
        if (Found) M2Data = *Found;
    }

    if (!M2Data)
    {
        TArray<uint8> M2Raw;
        if (!Mpq->ReadFile(ModelPath, M2Raw))
        {
            UE_LOG(LogWowCharacter, Warning, TEXT("Failed to read M2: %s"), *ModelPath);
            return nullptr;
        }

        FString SkinPath = ModelPath;
        if (SkinPath.EndsWith(TEXT(".m2"), ESearchCase::IgnoreCase))
            SkinPath = SkinPath.Left(SkinPath.Len() - 3);
        SkinPath += TEXT("00.skin");

        TArray<uint8> SkinRaw;
        Mpq->ReadFile(SkinPath, SkinRaw);

        M2Data = MakeShared<FM2Data>(FM2Parser::Parse(M2Raw, SkinRaw));
        if (!M2Data->bIsValid)
        {
            UE_LOG(LogWowCharacter, Warning, TEXT("Failed to parse M2: %s"), *ModelPath);
            return nullptr;
        }

        FScopeLock Lock(&CharCacheLock);
        CharM2Cache.Add(NormPath, M2Data);
    }

    if (M2Data->Vertices.Num() == 0 || M2Data->Indices.Num() == 0)
    {
        UE_LOG(LogWowCharacter, Warning, TEXT("M2 has no geometry: %s"), *ModelPath);
        return nullptr;
    }

    AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Rotation, Location));
    if (!Actor) return nullptr;

    USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
    Actor->SetRootComponent(Root);
    Root->RegisterComponent();

    if (M2Data->HasBones())
    {
        USkeleton* Skeleton = nullptr;
        {
            FScopeLock Lock(&CharCacheLock);
            if (TWeakObjectPtr<USkeleton>* CachedSkel = CharSkeletonCache.Find(NormPath))
            {
                Skeleton = CachedSkel->Get();
            }
        }

        if (!Skeleton)
        {
            Skeleton = FWowSkeletalMeshBuilder::CreateSkeleton(*M2Data, ModelPath);
            if (Skeleton)
            {
                FScopeLock Lock(&CharCacheLock);
                CharSkeletonCache.Add(NormPath, Skeleton);
            }
        }

        // Compute geoset visibility BEFORE building mesh
        TMap<uint16, uint16> VisibleGeosets;
        if (Customization)
        {
            VisibleGeosets = ComputeDefaultGeosets(*Customization);
        }

        // Build BODY mesh (excludes hair sections that need different texture)
        // Hair sections are identified by M2 texture type 6
        TMap<uint16, uint16> BodyGeosets = VisibleGeosets;
        TMap<uint16, uint16> HairOnlyGeosets;
        bool bHasHairSections = false;

        // Check if any render passes reference type 6 (hair) texture
        if (Customization && M2Data->TextureTypes.Num() > 0)
        {
            for (const FM2RenderPass& Pass : M2Data->RenderPasses)
            {
                if (Pass.TextureIndex < static_cast<uint16>(M2Data->TextureTypes.Num()) &&
                    M2Data->TextureTypes[Pass.TextureIndex] == 6)
                {
                    bHasHairSections = true;
                    break;
                }
            }
        }

        // Build body mesh
        const TMap<uint16, uint16>* GeosetFilter = Customization ? &BodyGeosets : nullptr;
        TArray<FGeosetSectionInfo> GeosetInfo;
        USkeletalMesh* SkeletalMesh = nullptr;
        if (Skeleton)
        {
            SkeletalMesh = FWowSkeletalMeshBuilder::CreateSkeletalMesh(*M2Data, Skeleton, ModelPath, Mpq, Cache, &GeosetInfo, GeosetFilter);
        }

        if (SkeletalMesh)
        {
            // Resolve skin texture
            UTexture2D* SkinTexture = OverrideTexture;
            if (!SkinTexture && M2Data->TexturePaths.Num() > 0 && !M2Data->TexturePaths[0].IsEmpty())
            {
                SkinTexture = LoadBlpTexture(Mpq, Cache, M2Data->TexturePaths[0]);
            }

            // Create body mesh component with skin texture
            USkeletalMeshComponent* SkelMesh = NewObject<USkeletalMeshComponent>(Actor, TEXT("CharacterMesh"));
            SkelMesh->SetupAttachment(Root);
            SkelMesh->SetSkeletalMesh(SkeletalMesh);
            SkelMesh->SetWorldScale3D(FVector(Scale));
            SkelMesh->SetCastShadow(true);
            SkelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

            if (UMaterial* BaseMat = GetCharacterMaterial())
            {
                for (int32 MatIdx = 0; MatIdx < SkeletalMesh->GetMaterials().Num(); ++MatIdx)
                {
                    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, SkelMesh);
                    if (SkinTexture) MID->SetTextureParameterValue(TEXT("BaseTexture"), SkinTexture);
                    SkelMesh->SetMaterial(MatIdx, MID);
                }
            }
            SkelMesh->RegisterComponent();

            // Build SEPARATE hair mesh if there are hair-textured sections
            if (bHasHairSections && Customization)
            {
                // Load hair texture
                UTexture2D* HairTexture = nullptr;
                for (const FCharSectionsDbcEntry& Entry : FDbcStore::Get().CharSections().GetAll())
                {
                    if (Entry.RaceID == Customization->RaceId && Entry.SexID == Customization->Gender &&
                        Entry.Type == 3 && Entry.Variation == Customization->HairStyle &&
                        Entry.Color == Customization->HairColor && !Entry.Textures[0].IsEmpty())
                    {
                        HairTexture = LoadBlpTexture(Mpq, Cache, Entry.Textures[0]);
                        break;
                    }
                }

                if (HairTexture)
                {
                    // Build hair-only mesh: include ONLY render passes with texture type 6
                    USkeletalMesh* HairMesh = FWowSkeletalMeshBuilder::CreateSkeletalMeshByTextureType(
                        *M2Data, Skeleton, ModelPath + TEXT("_hair"), Mpq, Cache, 6, GeosetFilter);

                    if (HairMesh)
                    {
                        USkeletalMeshComponent* HairComp = NewObject<USkeletalMeshComponent>(Actor, TEXT("HairMesh"));
                        HairComp->SetupAttachment(Root);
                        HairComp->SetSkeletalMesh(HairMesh);
                        HairComp->SetWorldScale3D(FVector(Scale));
                        HairComp->SetCastShadow(true);
                        HairComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                        HairComp->SetLeaderPoseComponent(SkelMesh); // Follow body animations

                        if (UMaterial* BaseMat = GetCharacterMaterial())
                        {
                            for (int32 MatIdx = 0; MatIdx < HairMesh->GetMaterials().Num(); ++MatIdx)
                            {
                                UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, HairComp);
                                MID->SetTextureParameterValue(TEXT("BaseTexture"), HairTexture);
                                HairComp->SetMaterial(MatIdx, MID);
                            }
                        }
                        HairComp->RegisterComponent();
                        UE_LOG(LogWowCharacter, Log, TEXT("Created separate hair mesh with texture: %s"),
                            *HairTexture->GetName());
                    }
                }
            }

            TArray<TWeakObjectPtr<UAnimSequence>> CachedAnimations;
            bool bNeedCreateAnimations = true;
            {
                FScopeLock Lock(&CharCacheLock);
                if (TArray<TWeakObjectPtr<UAnimSequence>>* Found = CharAnimCache.Find(NormPath))
                {
                    CachedAnimations = *Found;
                    bNeedCreateAnimations = Found->Num() == 0 || !(*Found)[0].IsValid();
                }
            }

            if (bNeedCreateAnimations && Skeleton)
            {
                CachedAnimations.Reset();
                for (UAnimSequence* Animation : FWowSkeletalMeshBuilder::CreateAnimations(*M2Data, Skeleton, ModelPath))
                {
                    if (Animation)
                    {
                        CachedAnimations.Add(Animation);
                    }
                }

                if (CachedAnimations.Num() == 0)
                {
                    if (UAnimSequence* ReferencePoseAnimation = CreateReferencePoseAnimation(Skeleton, ModelPath))
                    {
                        CachedAnimations.Add(ReferencePoseAnimation);
                    }
                }

                FScopeLock Lock(&CharCacheLock);
                CharAnimCache.Add(NormPath, CachedAnimations);
            }

            if (CachedAnimations.Num() > 0)
            {
                int32 AnimIndex = FindPreferredAnimationIndex(*M2Data);
                if (!CachedAnimations.IsValidIndex(AnimIndex) || !CachedAnimations[AnimIndex].IsValid())
                {
                    AnimIndex = 0;
                }

                if (CachedAnimations[AnimIndex].IsValid())
                {
                    SkelMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
                    SkelMesh->PlayAnimation(CachedAnimations[AnimIndex].Get(), true);
                }
            }

            UE_LOG(LogWowCharacter, Log, TEXT("Spawned skeletal M2 actor: %s at %s (scale=%.2f, %d bones, %d anims)"),
                *ModelPath, *Location.ToString(), Scale, M2Data->Bones.Num(), M2Data->AnimationTracks.Num());
            return Actor;
        }
    }

    UProceduralMeshComponent* ProcMesh = NewObject<UProceduralMeshComponent>(Actor, TEXT("CharacterMesh"));
    ProcMesh->SetupAttachment(Root);
    ProcMesh->SetWorldScale3D(FVector(Scale * FWowCoordinate::SCALE));
    ProcMesh->SetCastShadow(true);
    ProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Build mesh section from M2 vertex data
    const int32 NumVerts = M2Data->Vertices.Num();
    const int32 NumIndices = M2Data->Indices.Num();

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<int32> Triangles;

    Vertices.SetNum(NumVerts);
    Normals.SetNum(NumVerts);
    UVs.SetNum(NumVerts);
    Colors.SetNum(NumVerts);

    for (int32 i = 0; i < NumVerts; ++i)
    {
        const FM2Vertex& V = M2Data->Vertices[i];
        // Positions in WoW units (yards), component world scale handles UE conversion
        Vertices[i] = FVector(V.Position.Y, V.Position.X, V.Position.Z);
        Normals[i] = FVector(V.Normal.Y, V.Normal.X, V.Normal.Z).GetSafeNormal();
        UVs[i] = FVector2D(V.TexCoord.X, V.TexCoord.Y);
        Colors[i] = FColor::White;
    }

    Triangles.SetNum(NumIndices);
    for (int32 i = 0; i < NumIndices; ++i)
    {
        Triangles[i] = M2Data->Indices[i];
    }

    ProcMesh->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);

    // Load and apply texture
    UTexture2D* TexToApply = OverrideTexture;
    if (!TexToApply && M2Data->TexturePaths.Num() > 0 && !M2Data->TexturePaths[0].IsEmpty())
    {
        TexToApply = LoadBlpTexture(Mpq, Cache, M2Data->TexturePaths[0]);
    }

    ApplyTexturedMaterial(ProcMesh, TexToApply);

    ProcMesh->RegisterComponent();

    UE_LOG(LogWowCharacter, Log, TEXT("Spawned M2 actor: %s at %s (scale=%.1f, %d verts, %d tris)"),
        *ModelPath, *Location.ToString(), Scale, NumVerts, NumIndices / 3);

    return Actor;
}
