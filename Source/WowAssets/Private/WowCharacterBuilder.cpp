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
    UE_LOG(LogWowCharacter, Log, TEXT("Creating character material (UV0-based, DefaultLit)"));

    UPackage* MatPackage = CreatePackage(TEXT("/Game/Materials/M_WowCharacter"));
    CachedMat = NewObject<UMaterial>(MatPackage, TEXT("M_WowCharacter"),
        RF_Public | RF_Standalone);
    CachedMat->SetShadingModel(MSM_DefaultLit);
    CachedMat->BlendMode = BLEND_Opaque;
    CachedMat->TwoSided = false;
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

    CachedMat->GetEditorOnlyData()->BaseColor.Connect(0, TextureSampler);

    auto* RoughnessConst = NewObject<UMaterialExpressionConstant>(CachedMat);
    RoughnessConst->R = 0.8f;
    RoughnessConst->MaterialExpressionEditorX = 0;
    RoughnessConst->MaterialExpressionEditorY = 200;
    Expressions.AddExpression(RoughnessConst);
    CachedMat->GetEditorOnlyData()->Roughness.Connect(0, RoughnessConst);

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

    return SpawnM2Actor(World, Mpq, Cache, ModelPath, Location, Rotation, 1.0f, CompositeTex);
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

    // Spawn base character
    AActor* Actor = SpawnM2Actor(World, Mpq, Cache, ModelPath, Location, Rotation, 1.0f, CompositeTex);
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
    UTexture2D* OverrideTexture)
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

        USkeletalMesh* SkeletalMesh = Cache ? Cache->FindSkelMesh(NormPath) : nullptr;
        if (!SkeletalMesh && Skeleton)
        {
            SkeletalMesh = FWowSkeletalMeshBuilder::CreateSkeletalMesh(*M2Data, Skeleton, ModelPath, Mpq, Cache);
            if (SkeletalMesh && Cache)
            {
                Cache->CacheSkelMesh(NormPath, SkeletalMesh);
            }
        }

        if (SkeletalMesh)
        {
            USkeletalMeshComponent* SkelMesh = NewObject<USkeletalMeshComponent>(Actor, TEXT("CharacterMesh"));
            SkelMesh->SetupAttachment(Root);
            SkelMesh->SetSkeletalMesh(SkeletalMesh);
            SkelMesh->SetWorldScale3D(FVector(Scale));
            SkelMesh->SetCastShadow(true);
            SkelMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            SkelMesh->RegisterComponent();

            UTexture2D* TextureToApply = OverrideTexture;
            if (!TextureToApply && M2Data->TexturePaths.Num() > 0 && !M2Data->TexturePaths[0].IsEmpty())
            {
                TextureToApply = LoadBlpTexture(Mpq, Cache, M2Data->TexturePaths[0]);
            }
            ApplyTexturedMaterial(SkelMesh, TextureToApply);

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
