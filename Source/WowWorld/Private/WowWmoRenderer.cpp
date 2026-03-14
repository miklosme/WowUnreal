#include "WowWmoRenderer.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "Formats/WmoParser.h"
#include "Formats/WmoTypes.h"
#include "Formats/BlpParser.h"
#include "Formats/BlpTypes.h"
#include "WowTextureFactory.h"
#include "WowTerrainMaterial.h"
#include "Coord/WowCoordinate.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Formats/AdtTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowWmo, Log, All);

FString FWowWmoRenderer::GetGroupPath(const FString& RootPath, int32 GroupIndex)
{
    // Replace ".wmo" with "_XXX.wmo" where XXX is zero-padded group index
    FString Base = RootPath;
    if (Base.EndsWith(TEXT(".wmo"), ESearchCase::IgnoreCase))
    {
        Base = Base.Left(Base.Len() - 4);
    }
    return FString::Printf(TEXT("%s_%03d.wmo"), *Base, GroupIndex);
}

AActor* FWowWmoRenderer::SpawnWmo(UWorld* World, const FString& WmoPath, const FAdtWmoPlacement& Placement,
                                   FMpqManager* Mpq, FWowAssetCache* Cache)
{
    if (!World || !Mpq || WmoPath.IsEmpty())
    {
        return nullptr;
    }

    // Read and parse root WMO
    TArray<uint8> RootRaw;
    if (!Mpq->ReadFile(WmoPath, RootRaw))
    {
        UE_LOG(LogWowWmo, Verbose, TEXT("Failed to read WMO root: %s"), *WmoPath);
        return nullptr;
    }

    FWmoRootData RootData = FWmoParser::ParseRoot(RootRaw);
    if (!RootData.bIsValid || RootData.NumGroups == 0)
    {
        UE_LOG(LogWowWmo, Verbose, TEXT("Invalid WMO root or no groups: %s"), *WmoPath);
        return nullptr;
    }

    // Spawn actor for this WMO (use NAME_None to auto-generate unique name)
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* WmoActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
    if (!WmoActor)
    {
        return nullptr;
    }

    // Create root component
    USceneComponent* RootComp = NewObject<USceneComponent>(WmoActor, TEXT("WmoRoot"));
    WmoActor->SetRootComponent(RootComp);
    RootComp->RegisterComponent();

    // Position in ADT space
    FVector UEPos = FWowCoordinate::AdtToUE(Placement.Position.X, Placement.Position.Y, Placement.Position.Z);
    WmoActor->SetActorLocation(UEPos);

    // Full 3-axis rotation: WoW MODF rotation (Rx, Ry, Rz) → UE FRotator
    WmoActor->SetActorRotation(FWowCoordinate::WowRotationToUE(
        Placement.Rotation.X, Placement.Rotation.Y, Placement.Rotation.Z));

    // Scale (WMOs can have scale too)
    float ScaleVal = (Placement.Scale == 0) ? 1.0f : Placement.Scale / 1024.0f;
    WmoActor->SetActorScale3D(FVector(ScaleVal));

    int32 GroupsLoaded = 0;

    // Load each group as UStaticMesh with Nanite support
    for (uint32 GroupIdx = 0; GroupIdx < RootData.NumGroups; ++GroupIdx)
    {
        FString GroupPath = GetGroupPath(WmoPath, GroupIdx);

        TArray<uint8> GroupRaw;
        if (!Mpq->ReadFile(GroupPath, GroupRaw))
        {
            UE_LOG(LogWowWmo, Verbose, TEXT("Failed to read WMO group %d: %s"), GroupIdx, *GroupPath);
            continue;
        }

        FWmoGroupData GroupData = FWmoParser::ParseGroup(GroupRaw);
        if (!GroupData.bIsValid || GroupData.Vertices.Num() == 0 || GroupData.Indices.Num() == 0)
        {
            continue;
        }

        // Create UStaticMesh with Nanite for this group
        UStaticMesh* GroupMesh = CreateStaticMeshFromWmoGroup(GroupData, RootData, WmoPath, GroupIdx, Mpq, Cache);
        if (!GroupMesh) continue;

        FName CompName = *FString::Printf(TEXT("WmoGroup_%d"), GroupIdx);
        UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(WmoActor, CompName);
        MeshComp->SetupAttachment(RootComp);
        MeshComp->SetStaticMesh(GroupMesh);
        MeshComp->SetCastShadow(true);
        MeshComp->RegisterComponent();
        ++GroupsLoaded;
    }

    if (GroupsLoaded == 0)
    {
        WmoActor->Destroy();
        return nullptr;
    }

    UE_LOG(LogWowWmo, Log, TEXT("Spawned WMO %s with %d/%d groups at %s"),
        *WmoPath, GroupsLoaded, RootData.NumGroups, *UEPos.ToString());

    return WmoActor;
}

UStaticMesh* FWowWmoRenderer::CreateStaticMeshFromWmoGroup(
    const FWmoGroupData& GroupData, const FWmoRootData& RootData,
    const FString& WmoPath, int32 GroupIdx,
    FMpqManager* Mpq, FWowAssetCache* Cache)
{
    if (GroupData.Vertices.Num() == 0 || GroupData.Indices.Num() == 0)
    {
        return nullptr;
    }

    UStaticMesh* SM = NewObject<UStaticMesh>();
    SM->AddToRoot();

    FMeshDescription MeshDesc;
    FStaticMeshAttributes Attributes(MeshDesc);
    Attributes.Register();

    const int32 NumVerts = GroupData.Vertices.Num();

    TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
    TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
    TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
    TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();
    TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = Attributes.GetVertexInstanceColors();

    // Create vertices and instances
    TArray<FVertexInstanceID> VertexInstanceIDs;
    VertexInstanceIDs.SetNum(NumVerts);

    // If we have batches, create one polygon group per batch for separate materials
    int32 NumPolyGroups = FMath::Max(1, GroupData.Batches.Num());
    TArray<FPolygonGroupID> PolyGroups;
    for (int32 i = 0; i < NumPolyGroups; ++i)
    {
        PolyGroups.Add(MeshDesc.CreatePolygonGroup());
    }

    MeshDesc.ReserveNewVertices(NumVerts);
    MeshDesc.ReserveNewVertexInstances(NumVerts);

    for (int32 i = 0; i < NumVerts; ++i)
    {
        const FVector& P = GroupData.Vertices[i];
        FVertexID VertID = MeshDesc.CreateVertex();
        VertexPositions[VertID] = FVector3f(-P.X * FWowCoordinate::SCALE, P.Y * FWowCoordinate::SCALE, P.Z * FWowCoordinate::SCALE);

        FVertexInstanceID InstID = MeshDesc.CreateVertexInstance(VertID);
        VertexInstanceIDs[i] = InstID;

        FVector3f N = (i < GroupData.Normals.Num())
            ? FVector3f(-GroupData.Normals[i].X, GroupData.Normals[i].Y, GroupData.Normals[i].Z)
            : FVector3f(0, 0, 1);
        N.Normalize();
        VertexInstanceNormals[InstID] = N;

        FVector3f T = FVector3f::CrossProduct(N, FVector3f(0, 1, 0));
        if (T.SizeSquared() < 0.001f)
        {
            T = FVector3f::CrossProduct(N, FVector3f(1, 0, 0));
        }
        T.Normalize();
        VertexInstanceTangents[InstID] = T;
        VertexInstanceBinormalSigns[InstID] = 1.0f;

        FVector2f UV = (i < GroupData.TexCoords.Num())
            ? FVector2f(GroupData.TexCoords[i].X, GroupData.TexCoords[i].Y)
            : FVector2f(0, 0);
        VertexInstanceUVs.Set(InstID, 0, UV);

        if (i < GroupData.VertexColors.Num())
        {
            FLinearColor LC(GroupData.VertexColors[i]);
            VertexInstanceColors[InstID] = FVector4f(LC.R, LC.G, LC.B, LC.A);
        }
    }

    // Create triangles
    if (GroupData.Batches.Num() > 0)
    {
        for (int32 BatchIdx = 0; BatchIdx < GroupData.Batches.Num(); ++BatchIdx)
        {
            const FWmoGroupData::FBatch& Batch = GroupData.Batches[BatchIdx];
            for (uint16 j = 0; j + 2 < Batch.IndexCount; j += 3)
            {
                int32 Idx0 = Batch.IndexStart + j;
                int32 Idx1 = Batch.IndexStart + j + 1;
                int32 Idx2 = Batch.IndexStart + j + 2;
                if (Idx2 >= GroupData.Indices.Num()) break;

                TArray<FVertexInstanceID> TriVerts;
                TriVerts.Add(VertexInstanceIDs[GroupData.Indices[Idx0]]);
                TriVerts.Add(VertexInstanceIDs[GroupData.Indices[Idx1]]);
                TriVerts.Add(VertexInstanceIDs[GroupData.Indices[Idx2]]);
                MeshDesc.CreatePolygon(PolyGroups[BatchIdx], TriVerts);
            }
        }
    }
    else
    {
        int32 NumTris = GroupData.Indices.Num() / 3;
        for (int32 i = 0; i < NumTris; ++i)
        {
            TArray<FVertexInstanceID> TriVerts;
            TriVerts.Add(VertexInstanceIDs[GroupData.Indices[i * 3]]);
            TriVerts.Add(VertexInstanceIDs[GroupData.Indices[i * 3 + 1]]);
            TriVerts.Add(VertexInstanceIDs[GroupData.Indices[i * 3 + 2]]);
            MeshDesc.CreatePolygon(PolyGroups[0], TriVerts);
        }
    }

    // Build mesh
    TArray<const FMeshDescription*> MeshDescs;
    MeshDescs.Add(&MeshDesc);
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = true;
    SM->BuildFromMeshDescriptions(MeshDescs, Params);

    // Enable Nanite if available
    FMeshNaniteSettings NaniteSettings = SM->GetNaniteSettings();
    NaniteSettings.bEnabled = true;
    SM->SetNaniteSettings(NaniteSettings);

    // Apply materials per batch
    UMaterial* BaseMat = FWowTerrainMaterial::GetBaseMaterial();
    UMaterial* FallbackMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));

    if (GroupData.Batches.Num() > 0)
    {
        for (int32 BatchIdx = 0; BatchIdx < GroupData.Batches.Num(); ++BatchIdx)
        {
            const FWmoGroupData::FBatch& Batch = GroupData.Batches[BatchIdx];
            if (Batch.MaterialIndex < RootData.Materials.Num() && Cache && BaseMat)
            {
                const FWmoMaterial& WmoMat = RootData.Materials[Batch.MaterialIndex];
                UTexture2D* Tex = nullptr;
                if (!WmoMat.TexturePath1.IsEmpty())
                {
                    Tex = Cache->FindTexture(WmoMat.TexturePath1);
                    if (!Tex)
                    {
                        TArray<uint8> BlpRaw;
                        if (Mpq->ReadFile(WmoMat.TexturePath1, BlpRaw))
                        {
                            FBlpTexture BlpData = FBlpParser::Parse(BlpRaw);
                            if (BlpData.bIsValid)
                            {
                                Tex = FWowTextureFactory::CreateTexture(BlpData, WmoMat.TexturePath1);
                                if (Tex) Cache->CacheTexture(WmoMat.TexturePath1, Tex);
                            }
                        }
                    }
                }

                if (Tex)
                {
                    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, SM);
                    MID->SetTextureParameterValue(FName(TEXT("BaseTexture")), Tex);
                    SM->SetMaterial(BatchIdx, MID);
                }
                else if (FallbackMat)
                {
                    SM->SetMaterial(BatchIdx, FallbackMat);
                }
            }
            else if (FallbackMat)
            {
                SM->SetMaterial(BatchIdx, FallbackMat);
            }
        }
    }
    else if (FallbackMat)
    {
        SM->SetMaterial(0, FallbackMat);
    }

    return SM;
}

uint32 FWowWmoRenderer::GetWmoGroupCount(const FString& WmoPath, FMpqManager* Mpq)
{
    if (!Mpq || WmoPath.IsEmpty())
    {
        return 0;
    }

    TArray<uint8> RootRaw;
    if (!Mpq->ReadFile(WmoPath, RootRaw))
    {
        return 0;
    }

    FWmoRootData RootData = FWmoParser::ParseRoot(RootRaw);
    if (!RootData.bIsValid)
    {
        return 0;
    }

    return RootData.NumGroups;
}
