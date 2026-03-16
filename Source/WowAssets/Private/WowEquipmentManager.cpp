#include "WowEquipmentManager.h"
#include "WowSkeletalMeshBuilder.h"
#include "WowAssetCache.h"
#include "WowTextureFactory.h"
#include "Mpq/MpqManager.h"
#include "Formats/M2Parser.h"
#include "Formats/M2Types.h"
#include "Formats/BlpParser.h"
#include "Formats/BlpTypes.h"
#include "Formats/Dbc/DbcStore.h"
#include "Coord/WowCoordinate.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "UObject/UObjectGlobals.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWowEquip, Log, All);

namespace
{
FString NormalizeAssetPath(const FString& Path, const TCHAR* Extension)
{
    FString Result = Path;
    Result.ReplaceInline(TEXT("/"), TEXT("\\"));

    if (Result.EndsWith(TEXT(".mdx"), ESearchCase::IgnoreCase) && FCString::Stricmp(Extension, TEXT(".m2")) == 0)
    {
        Result = Result.LeftChop(4) + TEXT(".m2");
    }
    else if (!Result.EndsWith(Extension, ESearchCase::IgnoreCase))
    {
        Result += Extension;
    }

    return Result;
}

bool TryResolveAssetPath(FMpqManager* Mpq, const FString& FileName, const TCHAR* Extension,
    const TArray<FString>& CandidateDirs, FString& OutPath)
{
    if (!Mpq || FileName.IsEmpty())
    {
        return false;
    }

    TArray<FString> Candidates;
    Candidates.Reserve(1 + CandidateDirs.Num());

    const FString NormalizedFile = NormalizeAssetPath(FileName, Extension);
    Candidates.Add(NormalizedFile);

    for (const FString& Dir : CandidateDirs)
    {
        Candidates.Add(Dir + FPaths::GetCleanFilename(NormalizedFile));
    }

    for (const FString& Candidate : Candidates)
    {
        TArray<uint8> Raw;
        if (Mpq->ReadFile(Candidate, Raw))
        {
            OutPath = Candidate;
            return true;
        }
    }

    return false;
}

TArray<FString> GetCandidateDirs(FWowEquipmentManager::EAttachmentPoint AttachPoint)
{
    switch (AttachPoint)
    {
    case FWowEquipmentManager::EAttachmentPoint::Helmet:
        return {
            TEXT("Item\\ObjectComponents\\Head\\"),
            TEXT("Item\\ObjectComponents\\Helmet\\")
        };
    case FWowEquipmentManager::EAttachmentPoint::LeftShoulder:
    case FWowEquipmentManager::EAttachmentPoint::RightShoulder:
        return {
            TEXT("Item\\ObjectComponents\\Shoulder\\")
        };
    case FWowEquipmentManager::EAttachmentPoint::Back:
        return {
            TEXT("Item\\ObjectComponents\\Cape\\"),
            TEXT("Item\\ObjectComponents\\Cloak\\")
        };
    case FWowEquipmentManager::EAttachmentPoint::Shield:
        return {
            TEXT("Item\\ObjectComponents\\Shield\\"),
            TEXT("Item\\ObjectComponents\\Weapon\\")
        };
    default:
        return {
            TEXT("Item\\ObjectComponents\\Weapon\\"),
            TEXT("Item\\ObjectComponents\\Shield\\"),
            TEXT("Item\\ObjectComponents\\Bow\\"),
            TEXT("Item\\ObjectComponents\\Crossbow\\"),
            TEXT("Item\\ObjectComponents\\Gun\\"),
            TEXT("Item\\ObjectComponents\\Thrown\\"),
            TEXT("Item\\ObjectComponents\\Misc\\")
        };
    }
}

const FM2Attachment* FindAttachment(const FM2Data& M2Data, FWowEquipmentManager::EAttachmentPoint AttachPoint)
{
    const uint32 PointId = static_cast<uint32>(AttachPoint);

    if (PointId < static_cast<uint32>(M2Data.AttachmentLookup.Num()))
    {
        const int16 AttachIdx = M2Data.AttachmentLookup[PointId];
        if (AttachIdx >= 0 && AttachIdx < M2Data.Attachments.Num())
        {
            return &M2Data.Attachments[AttachIdx];
        }
    }

    for (const FM2Attachment& Attach : M2Data.Attachments)
    {
        if (Attach.Id == PointId)
        {
            return &Attach;
        }
    }

    return nullptr;
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

    FBlpTexture Blp = FBlpParser::Parse(BlpRaw);
    if (!Blp.bIsValid)
    {
        return nullptr;
    }

    UTexture2D* Texture = FWowTextureFactory::CreateTexture(Blp, TexturePath);
    if (Texture && Cache)
    {
        Cache->CacheTexture(TexturePath, Texture);
    }
    return Texture;
}

UMaterial* GetEquipmentMaterial()
{
    static TWeakObjectPtr<UMaterial> CachedMaterial;
    if (!CachedMaterial.IsValid())
    {
#if WITH_EDITOR
        UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_WowEquipmentRuntimeTransient"));
        if (Material)
        {
            Material->SetShadingModel(MSM_Unlit);
            Material->BlendMode = BLEND_Opaque;
            Material->TwoSided = false;

            UTexture2D* DefaultTexture = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture"));
            auto& Expressions = Material->GetExpressionCollection();

            UMaterialExpressionTextureSampleParameter2D* TextureSampler = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
            TextureSampler->ParameterName = TEXT("BaseTexture");
            TextureSampler->SamplerType = SAMPLERTYPE_Color;
            TextureSampler->Texture = DefaultTexture;
            Expressions.AddExpression(TextureSampler);

            Material->GetEditorOnlyData()->EmissiveColor.Connect(0, TextureSampler);

            bool bNeedsRecompile = false;
            Material->SetMaterialUsage(bNeedsRecompile, MATUSAGE_SkeletalMesh);
            Material->SetMaterialUsage(bNeedsRecompile, MATUSAGE_StaticMesh);
            Material->PreEditChange(nullptr);
            Material->PostEditChange();
            Material->ForceRecompileForRendering();
            CachedMaterial = Material;
        }
#endif

        if (!CachedMaterial.IsValid())
        {
            CachedMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
        }
    }
    return CachedMaterial.Get();
}

void ApplyEquipmentMaterial(UPrimitiveComponent* Component, UTexture2D* Texture)
{
    if (!Component || !Texture)
    {
        return;
    }

    if (UMaterial* BaseMat = GetEquipmentMaterial())
    {
        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, Component);
        MID->SetTextureParameterValue(TEXT("BaseTexture"), Texture);
        Component->SetMaterial(0, MID);
    }
}
}

FName FWowEquipmentManager::GetAttachmentBoneName(const FM2Data& M2Data, EAttachmentPoint AttachPoint)
{
    if (const FM2Attachment* Attachment = FindAttachment(M2Data, AttachPoint))
    {
        if (Attachment->Bone >= 0)
        {
            return FWowSkeletalMeshBuilder::GetBoneName(Attachment->Bone);
        }
    }

    UE_LOG(LogWowEquip, Warning, TEXT("Attachment point %d not found in M2 data"), static_cast<uint32>(AttachPoint));
    return NAME_None;
}

bool FWowEquipmentManager::ResolveEquipmentPaths(FMpqManager* Mpq, uint32 ItemDisplayId, EAttachmentPoint AttachPoint,
    FString& OutModelPath, FString* OutTexturePath)
{
    const FItemDisplayInfoDbcEntry* DisplayInfo = FDbcStore::Get().ItemDisplayInfo().GetById(ItemDisplayId);
    if (!DisplayInfo)
    {
        return false;
    }

    FString ModelFile;
    FString TextureFile;
    if (AttachPoint == EAttachmentPoint::LeftHand || AttachPoint == EAttachmentPoint::LeftShoulder)
    {
        ModelFile = DisplayInfo->ModelNames[1];
        TextureFile = DisplayInfo->ModelTextures[1];
        if (ModelFile.IsEmpty()) ModelFile = DisplayInfo->ModelNames[0];
        if (TextureFile.IsEmpty()) TextureFile = DisplayInfo->ModelTextures[0];
    }
    else
    {
        ModelFile = DisplayInfo->ModelNames[0];
        TextureFile = DisplayInfo->ModelTextures[0];
    }

    if (ModelFile.IsEmpty())
    {
        return false;
    }

    const TArray<FString> CandidateDirs = GetCandidateDirs(AttachPoint);
    if (!TryResolveAssetPath(Mpq, ModelFile, TEXT(".m2"), CandidateDirs, OutModelPath))
    {
        return false;
    }

    if (OutTexturePath)
    {
        OutTexturePath->Reset();
        if (!TextureFile.IsEmpty())
        {
            TryResolveAssetPath(Mpq, TextureFile, TEXT(".blp"), CandidateDirs, *OutTexturePath);
        }
    }

    return true;
}

USceneComponent* FWowEquipmentManager::AttachEquipment(FMpqManager* Mpq, FWowAssetCache* Cache,
    uint32 ItemDisplayId, EAttachmentPoint AttachPoint,
    USkeletalMeshComponent* CharacterMesh, const FM2Data& CharacterM2)
{
    if (!Mpq || !CharacterMesh) return nullptr;

    FString ModelPath;
    FString OverrideTexturePath;
    if (!ResolveEquipmentPaths(Mpq, ItemDisplayId, AttachPoint, ModelPath, &OverrideTexturePath))
    {
        UE_LOG(LogWowEquip, Verbose, TEXT("No equipment visual for display %d at attachment %d"),
            ItemDisplayId, static_cast<uint32>(AttachPoint));
        return nullptr;
    }

    FName BoneName = GetAttachmentBoneName(CharacterM2, AttachPoint);
    if (BoneName == NAME_None)
    {
        UE_LOG(LogWowEquip, Warning, TEXT("Cannot find bone for attachment point %d"),
            static_cast<uint32>(AttachPoint));
        return nullptr;
    }

    // Load the weapon M2
    TArray<uint8> M2Raw;
    if (!Mpq->ReadFile(ModelPath, M2Raw))
    {
        UE_LOG(LogWowEquip, Warning, TEXT("Failed to read equipment M2: %s"), *ModelPath);
        return nullptr;
    }

    FString SkinPath = ModelPath;
    if (SkinPath.EndsWith(TEXT(".m2"), ESearchCase::IgnoreCase))
        SkinPath = SkinPath.Left(SkinPath.Len() - 3);
    SkinPath += TEXT("00.skin");

    TArray<uint8> SkinRaw;
    Mpq->ReadFile(SkinPath, SkinRaw);

    FM2Data WeaponData = FM2Parser::Parse(M2Raw, SkinRaw);
    if (!WeaponData.bIsValid)
    {
        UE_LOG(LogWowEquip, Warning, TEXT("Failed to parse equipment M2: %s"), *ModelPath);
        return nullptr;
    }

    // Build a static mesh for the weapon (weapons don't animate independently)
    // Using the existing mesh building pipeline from doodad manager
    FString NormPath = ModelPath.ToLower();
    NormPath.ReplaceInline(TEXT("/"), TEXT("\\"));
    UStaticMesh* WeaponMesh = Cache ? Cache->FindMesh(NormPath) : nullptr;

    if (!WeaponMesh)
    {
        // Build static mesh from M2 data
        FMeshDescription MeshDesc;
        FStaticMeshAttributes MeshAttribs(MeshDesc);
        MeshAttribs.Register();

        if (WeaponData.Vertices.Num() > 0 && WeaponData.Indices.Num() > 0)
        {
            MeshDesc.ReserveNewVertices(WeaponData.Vertices.Num());
            FPolygonGroupID PGId = MeshDesc.CreatePolygonGroup();

            TVertexAttributesRef<FVector3f> Positions = MeshAttribs.GetVertexPositions();
            TVertexInstanceAttributesRef<FVector3f> Normals = MeshAttribs.GetVertexInstanceNormals();
            TVertexInstanceAttributesRef<FVector2f> UVs = MeshAttribs.GetVertexInstanceUVs();

            TArray<FVertexID> VertexIds;
            VertexIds.SetNum(WeaponData.Vertices.Num());
            for (int32 v = 0; v < WeaponData.Vertices.Num(); v++)
            {
                VertexIds[v] = MeshDesc.CreateVertex();
                const FM2Vertex& Vertex = WeaponData.Vertices[v];
                Positions[VertexIds[v]] = FVector3f(
                    Vertex.Position.Y * FWowCoordinate::SCALE,
                    Vertex.Position.X * FWowCoordinate::SCALE,
                    Vertex.Position.Z * FWowCoordinate::SCALE);
            }

            for (int32 t = 0; t + 2 < WeaponData.Indices.Num(); t += 3)
            {
                TArray<FVertexInstanceID> Insts;
                for (int32 j = 0; j < 3; j++)
                {
                    int32 Idx = WeaponData.Indices[t + j];
                    if (Idx >= WeaponData.Vertices.Num()) continue;
                    FVertexInstanceID VI = MeshDesc.CreateVertexInstance(VertexIds[Idx]);
                    const FM2Vertex& Vertex = WeaponData.Vertices[Idx];
                    FVector3f Normal(Vertex.Normal.Y, Vertex.Normal.X, Vertex.Normal.Z);
                    Normal.Normalize();
                    Normals[VI] = Normal;
                    UVs[VI] = FVector2f(Vertex.TexCoord.X, Vertex.TexCoord.Y);
                    Insts.Add(VI);
                }
                if (Insts.Num() == 3)
                {
                    MeshDesc.CreatePolygon(PGId, Insts);
                }
            }

            WeaponMesh = NewObject<UStaticMesh>();
            TArray<const FMeshDescription*> MeshDescs;
            MeshDescs.Add(&MeshDesc);
            UStaticMesh::FBuildMeshDescriptionsParams Params;
            Params.bFastBuild = true;
            WeaponMesh->BuildFromMeshDescriptions(MeshDescs, Params);
            if (WeaponMesh->GetStaticMaterials().Num() == 0)
            {
                WeaponMesh->GetStaticMaterials().Add(FStaticMaterial(UMaterial::GetDefaultMaterial(MD_Surface)));
            }

            if (Cache) Cache->CacheMesh(NormPath, WeaponMesh);
        }
    }

    if (!WeaponMesh) return nullptr;

    // Attach to character bone
    AActor* Owner = CharacterMesh->GetOwner();
    UStaticMeshComponent* WeaponComp = NewObject<UStaticMeshComponent>(Owner,
        *FString::Printf(TEXT("Equip_%d"), ItemDisplayId));
    WeaponComp->SetStaticMesh(WeaponMesh);
    WeaponComp->SetupAttachment(CharacterMesh, BoneName);
    WeaponComp->SetCastShadow(true);
    WeaponComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Attachment Position is in MODEL SPACE, not bone-relative.
    // The bone already provides the correct animated position.
    // Equipment attaches directly at the bone origin.
    WeaponComp->SetRelativeLocation(FVector::ZeroVector);

    WeaponComp->RegisterComponent();

    UTexture2D* EquipmentTexture = nullptr;
    if (!OverrideTexturePath.IsEmpty())
    {
        EquipmentTexture = LoadBlpTexture(Mpq, Cache, OverrideTexturePath);
    }

    if (!EquipmentTexture && WeaponData.TexturePaths.Num() > 0 && !WeaponData.TexturePaths[0].IsEmpty())
    {
        FString TexPath = WeaponData.TexturePaths[0];
        if (!TexPath.EndsWith(TEXT(".blp"), ESearchCase::IgnoreCase))
        {
            TexPath += TEXT(".blp");
        }
        EquipmentTexture = LoadBlpTexture(Mpq, Cache, TexPath);
    }

    ApplyEquipmentMaterial(WeaponComp, EquipmentTexture);

    UE_LOG(LogWowEquip, Log, TEXT("Attached equipment %d (%s) to bone %s"),
        ItemDisplayId, *ModelPath, *BoneName.ToString());

    return WeaponComp;
}
