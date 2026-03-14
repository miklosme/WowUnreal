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
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowEquip, Log, All);

FName FWowEquipmentManager::GetAttachmentBoneName(const FM2Data& M2Data, EAttachmentPoint AttachPoint)
{
    uint32 PointId = static_cast<uint32>(AttachPoint);

    // Search attachment lookup table first
    if (PointId < static_cast<uint32>(M2Data.AttachmentLookup.Num()))
    {
        int16 AttachIdx = M2Data.AttachmentLookup[PointId];
        if (AttachIdx >= 0 && AttachIdx < M2Data.Attachments.Num())
        {
            int32 BoneIdx = M2Data.Attachments[AttachIdx].Bone;
            if (BoneIdx >= 0)
            {
                return FWowSkeletalMeshBuilder::GetBoneName(BoneIdx);
            }
        }
    }

    // Fallback: search attachments directly
    for (const FM2Attachment& Attach : M2Data.Attachments)
    {
        if (Attach.Id == PointId && Attach.Bone >= 0)
        {
            return FWowSkeletalMeshBuilder::GetBoneName(Attach.Bone);
        }
    }

    UE_LOG(LogWowEquip, Warning, TEXT("Attachment point %d not found in M2 data"), PointId);
    return NAME_None;
}

USceneComponent* FWowEquipmentManager::AttachEquipment(FMpqManager* Mpq, FWowAssetCache* Cache,
    uint32 ItemDisplayId, EAttachmentPoint AttachPoint,
    USkeletalMeshComponent* CharacterMesh, const FM2Data& CharacterM2)
{
    if (!Mpq || !CharacterMesh) return nullptr;

    // Look up item model from ItemDisplayInfo.dbc
    const FItemDisplayInfoDbcEntry* DisplayInfo = FDbcStore::Get().ItemDisplayInfo().GetById(ItemDisplayId);
    if (!DisplayInfo)
    {
        UE_LOG(LogWowEquip, Warning, TEXT("ItemDisplayInfo %d not found"), ItemDisplayId);
        return nullptr;
    }

    // Determine which model file to use based on attachment point
    // ModelNames[0] = right-hand/main model, ModelNames[1] = left-hand/secondary model
    FString ModelFile;
    if (AttachPoint == EAttachmentPoint::LeftHand || AttachPoint == EAttachmentPoint::LeftShoulder)
    {
        ModelFile = DisplayInfo->ModelNames[1];
        if (ModelFile.IsEmpty()) ModelFile = DisplayInfo->ModelNames[0];
    }
    else
    {
        ModelFile = DisplayInfo->ModelNames[0];
    }

    if (ModelFile.IsEmpty())
    {
        UE_LOG(LogWowEquip, Verbose, TEXT("No model for ItemDisplayInfo %d at attachment %d"),
            ItemDisplayId, static_cast<uint32>(AttachPoint));
        return nullptr;
    }

    // Build full path — item models are under Item/ObjectComponents/
    FString ModelPath = FString::Printf(TEXT("Item\\ObjectComponents\\Weapon\\%s"), *ModelFile);
    if (!ModelPath.EndsWith(TEXT(".m2"), ESearchCase::IgnoreCase))
    {
        ModelPath += TEXT(".m2");
    }

    // Find the bone to attach to
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
                Positions[VertexIds[v]] = FVector3f(WeaponData.Vertices[v].Position);
            }

            for (int32 t = 0; t + 2 < WeaponData.Indices.Num(); t += 3)
            {
                TArray<FVertexInstanceID> Insts;
                for (int32 j = 0; j < 3; j++)
                {
                    int32 Idx = WeaponData.Indices[t + j];
                    if (Idx >= WeaponData.Vertices.Num()) continue;
                    FVertexInstanceID VI = MeshDesc.CreateVertexInstance(VertexIds[Idx]);
                    Normals[VI] = FVector3f(WeaponData.Vertices[Idx].Normal);
                    UVs[VI] = FVector2f(WeaponData.Vertices[Idx].TexCoord);
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
            WeaponMesh->AddToRoot();

            // Load texture
            if (WeaponData.TexturePaths.Num() > 0 && !WeaponData.TexturePaths[0].IsEmpty())
            {
                FString TexPath = WeaponData.TexturePaths[0];
                if (!TexPath.EndsWith(TEXT(".blp"), ESearchCase::IgnoreCase))
                    TexPath += TEXT(".blp");

                TArray<uint8> BlpRaw;
                if (Mpq->ReadFile(TexPath, BlpRaw))
                {
                    FBlpTexture Blp = FBlpParser::Parse(BlpRaw);
                    if (Blp.bIsValid)
                    {
                        UTexture2D* Tex = FWowTextureFactory::CreateTexture(Blp, TexPath);
                        if (Tex)
                        {
                            UMaterial* BaseMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial"));
                            if (BaseMat)
                            {
                                UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, nullptr);
                                MID->SetTextureParameterValue(TEXT("BaseColor"), Tex);
                                WeaponMesh->SetMaterial(0, MID);
                            }
                        }
                    }
                }
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
    WeaponComp->RegisterComponent();

    UE_LOG(LogWowEquip, Log, TEXT("Attached equipment %d (%s) to bone %s"),
        ItemDisplayId, *ModelPath, *BoneName.ToString());

    return WeaponComp;
}
