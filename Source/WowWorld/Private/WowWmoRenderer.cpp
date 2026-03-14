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
#include "ProceduralMeshComponent.h"
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

    // MODF positions: convert to ADT space (same as terrain), then use AdtToUE
    // ADT space: X=east, Y=up, Z=south. MODF stores as map-coords where
    // adtX = MAP_ORIGIN - pos[0], adtZ = MAP_ORIGIN - pos[2], adtY = pos[1]
    float AdtX = FWowCoordinate::MAP_ORIGIN - Placement.Position.X;
    float AdtY = Placement.Position.Y;
    float AdtZ = FWowCoordinate::MAP_ORIGIN - Placement.Position.Z;
    FVector UEPos = FWowCoordinate::AdtToUE(AdtX, AdtY, AdtZ);

    // Rotation: from WowGodot — (rotX, rotY-90, -rotZ) as YXZ euler
    // UE uses Pitch(Y), Yaw(Z), Roll(X) but FRotator is (Pitch, Yaw, Roll)
    FRotator UERot = FRotator(Placement.Rotation.X, Placement.Rotation.Y - 90.0f, -Placement.Rotation.Z);

    WmoActor->SetActorLocation(UEPos);
    WmoActor->SetActorRotation(UERot);

    // Scale (WMOs can have scale too)
    float ScaleVal = (Placement.Scale == 0) ? 1.0f : Placement.Scale / 1024.0f;
    WmoActor->SetActorScale3D(FVector(ScaleVal));

    // Get the runtime material that supports texture parameters
    UMaterial* BaseMat = FWowTerrainMaterial::GetBaseMaterial();
    UMaterial* FallbackMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));

    int32 GroupsLoaded = 0;

    // Load each group
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

        FName CompName = *FString::Printf(TEXT("WmoGroup_%d"), GroupIdx);
        UProceduralMeshComponent* MeshComp = NewObject<UProceduralMeshComponent>(WmoActor, CompName);
        MeshComp->SetupAttachment(RootComp);

        const int32 NumVerts = GroupData.Vertices.Num();

        // Convert WMO vertices from WoW model space to UE local space
        TArray<FVector> Vertices;
        TArray<FVector> Normals;
        TArray<FVector2D> UVs;
        TArray<FProcMeshTangent> Tangents;
        TArray<FLinearColor> VertColors;

        Vertices.SetNum(NumVerts);
        Normals.SetNum(NumVerts);
        Tangents.SetNum(NumVerts);

        // UVs - use what we have or generate defaults
        if (GroupData.TexCoords.Num() == NumVerts)
        {
            UVs = GroupData.TexCoords;
        }
        else
        {
            UVs.SetNumZeroed(NumVerts);
        }

        // Vertex colors
        if (GroupData.VertexColors.Num() == NumVerts)
        {
            VertColors.SetNum(NumVerts);
            for (int32 i = 0; i < NumVerts; ++i)
            {
                VertColors[i] = FLinearColor(GroupData.VertexColors[i]);
            }
        }

        for (int32 i = 0; i < NumVerts; ++i)
        {
            // WoW file vertices → Godot: wow_to_godot(x,y,z)=(y,z,-x), then negate Z → (y,z,x)
            // Godot(Y-up) → UE(Z-up): UE = (godotX, -godotZ, godotY) = (fileY, -fileX, fileZ)
            const FVector& P = GroupData.Vertices[i];
            Vertices[i] = FVector(P.Y, -P.X, P.Z) * FWowCoordinate::SCALE;

            FVector N = (i < GroupData.Normals.Num())
                ? FVector(GroupData.Normals[i].Y, -GroupData.Normals[i].X, GroupData.Normals[i].Z)
                : FVector(0, 0, 1);
            N.Normalize();
            Normals[i] = N;

            // Approximate tangent
            FVector T = FVector::CrossProduct(N, FVector(0, 1, 0));
            if (T.SizeSquared() < 0.001f)
            {
                T = FVector::CrossProduct(N, FVector(1, 0, 0));
            }
            T.Normalize();
            Tangents[i] = FProcMeshTangent(T, false);
        }

        // Convert indices - no winding reversal needed (cyclic permutation has det=+1)
        TArray<int32> Indices;
        Indices.SetNum(GroupData.Indices.Num());
        for (int32 i = 0; i < GroupData.Indices.Num(); ++i)
        {
            Indices[i] = static_cast<int32>(GroupData.Indices[i]);
        }

        // If we have batches, create one section per batch for separate materials
        if (GroupData.Batches.Num() > 0)
        {
            int32 SectionIdx = 0;
            for (const FWmoGroupData::FBatch& Batch : GroupData.Batches)
            {
                if (Batch.IndexCount == 0) continue;

                // Extract sub-mesh for this batch
                TArray<FVector> BatchVerts;
                TArray<FVector> BatchNormals;
                TArray<FVector2D> BatchUVs;
                TArray<FProcMeshTangent> BatchTangents;
                TArray<FLinearColor> BatchColors;
                TArray<int32> BatchIndices;

                // Remap: extract the vertex range used by this batch
                TMap<int32, int32> VertexRemap;
                int32 NextVert = 0;

                for (uint16 j = 0; j < Batch.IndexCount; ++j)
                {
                    int32 SrcIdx = Batch.IndexStart + j;
                    if (SrcIdx >= Indices.Num()) break;

                    int32 OrigVert = Indices[SrcIdx];
                    int32* RemappedIdx = VertexRemap.Find(OrigVert);
                    if (RemappedIdx)
                    {
                        BatchIndices.Add(*RemappedIdx);
                    }
                    else
                    {
                        int32 NewIdx = NextVert++;
                        VertexRemap.Add(OrigVert, NewIdx);
                        BatchIndices.Add(NewIdx);

                        if (OrigVert < Vertices.Num())
                        {
                            BatchVerts.Add(Vertices[OrigVert]);
                            BatchNormals.Add(Normals[OrigVert]);
                            BatchUVs.Add(UVs[OrigVert]);
                            BatchTangents.Add(Tangents[OrigVert]);
                            if (VertColors.Num() > OrigVert)
                            {
                                BatchColors.Add(VertColors[OrigVert]);
                            }
                        }
                    }
                }

                if (BatchVerts.Num() == 0 || BatchIndices.Num() == 0) continue;

                MeshComp->CreateMeshSection_LinearColor(
                    SectionIdx, BatchVerts, BatchIndices, BatchNormals,
                    BatchUVs, BatchColors, BatchTangents, false);

                // Apply textured material from WMO material data
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
                        UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, WmoActor);
                        MID->SetTextureParameterValue(FName(TEXT("BaseTexture")), Tex);
                        MeshComp->SetMaterial(SectionIdx, MID);
                    }
                    else if (FallbackMat)
                    {
                        MeshComp->SetMaterial(SectionIdx, FallbackMat);
                    }
                }

                ++SectionIdx;
            }
        }
        else
        {
            // No batches - single section for the whole group
            MeshComp->CreateMeshSection_LinearColor(
                0, Vertices, Indices, Normals, UVs, VertColors, Tangents, false);

            if (FallbackMat)
            {
                MeshComp->SetMaterial(0, FallbackMat);
            }
        }

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
