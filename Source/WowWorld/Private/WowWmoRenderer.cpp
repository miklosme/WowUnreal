#include "WowWmoRenderer.h"
#include "WowWaterMaterial.h"
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

        // Create liquid mesh for this group if MLIQ data exists
        if (GroupData.Liquid.HasLiquid())
        {
            const FWmoLiquidData& Liq = GroupData.Liquid;
            uint32 XTiles = Liq.XVerts - 1;
            uint32 YTiles = Liq.YVerts - 1;

            TArray<FVector3f> LiqVerts;
            TArray<FVector2f> LiqUVs;
            TArray<FVector4f> LiqColors;
            TArray<int32> LiqIndices;
            LiqVerts.Reserve(Liq.XVerts * Liq.YVerts);
            LiqUVs.Reserve(Liq.XVerts * Liq.YVerts);
            LiqColors.Reserve(Liq.XVerts * Liq.YVerts);

            int32 Category = Liq.GetLiquidCategory();

            for (uint32 y = 0; y < Liq.YVerts; y++)
            {
                for (uint32 x = 0; x < Liq.XVerts; x++)
                {
                    uint32 Idx = y * Liq.XVerts + x;
                    float Height = (Idx < (uint32)Liq.Vertices.Num()) ? Liq.Vertices[Idx].Height : 0.0f;

                    // MLIQ grid in WMO model space (X=east, Y=north, Z=up)
                    constexpr float LIQUID_UNIT = 4.1667f;
                    float ModelX = Liq.Position.X + LIQUID_UNIT * x;
                    float ModelY = Liq.Position.Y + LIQUID_UNIT * y;
                    float ModelZ = Height;

                    // WMO model → UE local: (Y, X, Z) * SCALE (same as group mesh)
                    LiqVerts.Add(FVector3f(ModelY * FWowCoordinate::SCALE,
                                           ModelX * FWowCoordinate::SCALE,
                                           ModelZ * FWowCoordinate::SCALE));

                    float U = (float)x / FMath::Max(1u, Liq.XVerts - 1);
                    float V = (float)y / FMath::Max(1u, Liq.YVerts - 1);
                    LiqUVs.Add(FVector2f(U, V));

                    // Depth opacity from flow data
                    float DepthAlpha = 0.7f;
                    if (Idx < (uint32)Liq.Vertices.Num())
                    {
                        DepthAlpha = FMath::Lerp(0.3f, 0.9f, Liq.Vertices[Idx].Flow1 / 255.0f);
                    }
                    if (Category == 2 || Category == 3) DepthAlpha = 1.0f;

                    FVector4f VC;
                    switch (Category)
                    {
                    case 0: case 1: VC = FVector4f(0.1f, 0.3f, 0.6f, DepthAlpha); break;
                    case 2: VC = FVector4f(1.0f, 0.4f, 0.1f, 1.0f); break;
                    case 3: VC = FVector4f(0.2f, 0.7f, 0.1f, 1.0f); break;
                    default: VC = FVector4f(0.1f, 0.3f, 0.6f, DepthAlpha); break;
                    }
                    LiqColors.Add(VC);
                }
            }

            // Build triangles, respecting tile visibility flags
            for (uint32 y = 0; y < YTiles; y++)
            {
                for (uint32 x = 0; x < XTiles; x++)
                {
                    uint32 TileIdx = y * XTiles + x;
                    if (TileIdx < (uint32)Liq.TileFlags.Num())
                    {
                        // Bit 3 (0x08) means tile is invisible
                        if (Liq.TileFlags[TileIdx] & 0x08) continue;
                    }

                    int32 TL = y * Liq.XVerts + x;
                    int32 TR = TL + 1;
                    int32 BL = TL + Liq.XVerts;
                    int32 BR = BL + 1;

                    LiqIndices.Add(TL); LiqIndices.Add(BL); LiqIndices.Add(TR);
                    LiqIndices.Add(TR); LiqIndices.Add(BL); LiqIndices.Add(BR);
                }
            }

            if (LiqIndices.Num() > 0)
            {
                // Build liquid static mesh via FMeshDescription
                FMeshDescription LiqMeshDesc;
                FStaticMeshAttributes LiqAttrs(LiqMeshDesc);
                LiqAttrs.Register();

                auto LiqVertPos = LiqAttrs.GetVertexPositions();
                auto LiqInstNormals = LiqAttrs.GetVertexInstanceNormals();
                auto LiqInstTangents = LiqAttrs.GetVertexInstanceTangents();
                auto LiqInstBinSigns = LiqAttrs.GetVertexInstanceBinormalSigns();
                auto LiqInstUVs = LiqAttrs.GetVertexInstanceUVs();
                auto LiqInstColors = LiqAttrs.GetVertexInstanceColors();

                FPolygonGroupID LiqPolyGroup = LiqMeshDesc.CreatePolygonGroup();
                LiqMeshDesc.ReserveNewVertices(LiqVerts.Num());
                LiqMeshDesc.ReserveNewVertexInstances(LiqVerts.Num());
                LiqMeshDesc.ReserveNewPolygons(LiqIndices.Num() / 3);

                TArray<FVertexInstanceID> LiqInstIDs;
                LiqInstIDs.SetNum(LiqVerts.Num());

                for (int32 v = 0; v < LiqVerts.Num(); v++)
                {
                    FVertexID VID = LiqMeshDesc.CreateVertex();
                    LiqVertPos[VID] = LiqVerts[v];
                    FVertexInstanceID IID = LiqMeshDesc.CreateVertexInstance(VID);
                    LiqInstIDs[v] = IID;
                    LiqInstNormals[IID] = FVector3f(0, 0, 1);
                    LiqInstTangents[IID] = FVector3f(1, 0, 0);
                    LiqInstBinSigns[IID] = 1.0f;
                    LiqInstUVs.Set(IID, 0, (v < LiqUVs.Num()) ? LiqUVs[v] : FVector2f(0, 0));
                    LiqInstColors[IID] = (v < LiqColors.Num()) ? LiqColors[v] : FVector4f(1, 1, 1, 0.7f);
                }

                for (int32 t = 0; t < LiqIndices.Num(); t += 3)
                {
                    TArray<FVertexInstanceID> Tri;
                    Tri.Add(LiqInstIDs[LiqIndices[t]]);
                    Tri.Add(LiqInstIDs[LiqIndices[t + 1]]);
                    Tri.Add(LiqInstIDs[LiqIndices[t + 2]]);
                    LiqMeshDesc.CreatePolygon(LiqPolyGroup, Tri);
                }

                UStaticMesh* LiqSM = NewObject<UStaticMesh>();
                TArray<const FMeshDescription*> LiqDescs;
                LiqDescs.Add(&LiqMeshDesc);
                UStaticMesh::FBuildMeshDescriptionsParams LiqBuild;
                LiqBuild.bBuildSimpleCollision = false;
                LiqBuild.bFastBuild = true;
                LiqSM->BuildFromMeshDescriptions(LiqDescs, LiqBuild);

                UMaterial* LiqMat = FWowWaterMaterial::GetMaterialForCategory(Category);
                if (LiqMat)
                {
                    if (LiqSM->GetStaticMaterials().Num() <= 0)
                    {
                        LiqSM->GetStaticMaterials().SetNum(1);
                    }
                    LiqSM->GetStaticMaterials()[0].MaterialInterface = LiqMat;
                }

                FName LiqCompName = *FString::Printf(TEXT("WmoLiquid_%d"), GroupIdx);
                UStaticMeshComponent* LiqComp = NewObject<UStaticMeshComponent>(WmoActor, LiqCompName);
                LiqComp->SetupAttachment(RootComp);
                LiqComp->SetStaticMesh(LiqSM);
                LiqComp->SetCastShadow(false);
                LiqComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                LiqComp->RegisterComponent();

                UE_LOG(LogWowWmo, Log, TEXT("  Created WMO liquid mesh for group %d (%d tris, cat=%d)"),
                    GroupIdx, LiqIndices.Num() / 3, Category);
            }
        }
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
        // WMO model space → UE local space: (Y, X, Z) * SCALE
        // Derivation: WMO→ADT is (X,Z,-Y) [noggit3], ADT→UE is (-Z,X,Y)
        // Combined: UE.X=WMO.Y, UE.Y=WMO.X, UE.Z=WMO.Z (det=-1 handles RH→LH winding)
        VertexPositions[VertID] = FVector3f(P.Y * FWowCoordinate::SCALE, P.X * FWowCoordinate::SCALE, P.Z * FWowCoordinate::SCALE);

        FVertexInstanceID InstID = MeshDesc.CreateVertexInstance(VertID);
        VertexInstanceIDs[i] = InstID;

        FVector3f N = (i < GroupData.Normals.Num())
            ? FVector3f(GroupData.Normals[i].Y, GroupData.Normals[i].X, GroupData.Normals[i].Z)
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

    // Build materials BEFORE mesh so sections are initialized correctly
    UMaterial* BaseMat = FWowTerrainMaterial::GetSimpleObjectMaterial();
    UMaterial* FallbackMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));

    TArray<UMaterialInterface*> SectionMaterials;
    if (GroupData.Batches.Num() > 0)
    {
        for (int32 BatchIdx = 0; BatchIdx < GroupData.Batches.Num(); ++BatchIdx)
        {
            const FWmoGroupData::FBatch& Batch = GroupData.Batches[BatchIdx];
            UMaterialInterface* Mat = FallbackMat;

            if (Batch.MaterialIndex < RootData.Materials.Num() && Cache && BaseMat)
            {
                const FWmoMaterial& WmoMat = RootData.Materials[Batch.MaterialIndex];
                UTexture2D* Tex = nullptr;
                if (!WmoMat.TexturePath1.IsEmpty())
                {
                    Tex = FWowTerrainMaterial::LoadBlpTexture(WmoMat.TexturePath1, Mpq, Cache);
                }

                if (Tex)
                {
                    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, SM);
                    MID->SetTextureParameterValue(FName(TEXT("Layer0Texture")), Tex);
                    Mat = MID;
                }
            }
            SectionMaterials.Add(Mat);
        }
    }
    else
    {
        SectionMaterials.Add(FallbackMat);
    }

    // Add materials to static mesh before building
    for (UMaterialInterface* Mat : SectionMaterials)
    {
        SM->GetStaticMaterials().Add(FStaticMaterial(Mat));
    }

    // Build mesh
    TArray<const FMeshDescription*> MeshDescs;
    MeshDescs.Add(&MeshDesc);
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = true;
    Params.bCommitMeshDescription = true;
    SM->BuildFromMeshDescriptions(MeshDescs, Params);

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
