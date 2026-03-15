#include "WowSkeletalMeshBuilder.h"
#include "Formats/M2Types.h"
#include "Formats/BlpParser.h"
#include "Formats/BlpTypes.h"
#include "WowTextureFactory.h"
#include "WowAssetCache.h"
#include "Mpq/MpqManager.h"
#include "Coord/WowCoordinate.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "MeshUtilities.h"
#include "RenderingThread.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowSkelMesh, Log, All);

FName FWowSkeletalMeshBuilder::GetBoneName(int32 BoneIndex)
{
	return FName(*FString::Printf(TEXT("Bone_%03d"), BoneIndex));
}

USkeleton* FWowSkeletalMeshBuilder::CreateSkeleton(const FM2Data& Data, const FString& ModelName)
{
	if (Data.Bones.Num() == 0)
	{
		return nullptr;
	}

	USkeleton* Skeleton = NewObject<USkeleton>();

	const int32 NumBones = Data.Bones.Num();

	// UE requires bones in topological order (parent index < child index) with
	// exactly ONE root bone at index 0. M2 models may have multiple roots or
	// non-topological ordering, so we sort and fix parent references.
	TArray<int32> SortedOrder; // SortedOrder[new_index] = original_index
	TArray<int32> RemapTable;  // RemapTable[original_index] = new_index
	SortedOrder.Reserve(NumBones);
	RemapTable.SetNumUninitialized(NumBones);

	// Build children lists and find roots
	TArray<TArray<int32>> Children;
	Children.SetNum(NumBones);
	TArray<int32> Roots;
	TSet<int32> IsExtraRoot;

	for (int32 i = 0; i < NumBones; i++)
	{
		int32 Parent = Data.Bones[i].ParentBone;
		if (Parent >= 0 && Parent < NumBones && Parent != i)
		{
			Children[Parent].Add(i);
		}
		else
		{
			Roots.Add(i);
		}
	}

	// UE allows only one root bone. Reparent extra roots under the first.
	int32 PrimaryRoot = Roots.Num() > 0 ? Roots[0] : 0;
	for (int32 r = 1; r < Roots.Num(); r++)
	{
		IsExtraRoot.Add(Roots[r]);
		Children[PrimaryRoot].Add(Roots[r]);
	}

	// BFS from primary root (extra roots reachable via reparented edges)
	SortedOrder.Add(PrimaryRoot);
	int32 Head = 0;
	while (Head < SortedOrder.Num())
	{
		int32 Idx = SortedOrder[Head++];
		for (int32 Child : Children[Idx])
		{
			SortedOrder.Add(Child);
		}
	}

	// Add any unreachable orphans
	if (SortedOrder.Num() < NumBones)
	{
		TSet<int32> Added(SortedOrder);
		for (int32 i = 0; i < NumBones; i++)
		{
			if (!Added.Contains(i))
			{
				IsExtraRoot.Add(i);
				SortedOrder.Add(i);
			}
		}
	}

	for (int32 NewIdx = 0; NewIdx < NumBones; NewIdx++)
	{
		RemapTable[SortedOrder[NewIdx]] = NewIdx;
	}

	// Add bones in topological order using FReferenceSkeletonModifier
	{
		FReferenceSkeletonModifier SkelMod(Skeleton);
		for (int32 NewIdx = 0; NewIdx < NumBones; NewIdx++)
		{
			int32 OrigIdx = SortedOrder[NewIdx];
			const FM2Bone& Bone = Data.Bones[OrigIdx];
			FName BoneName = GetBoneName(OrigIdx);

			int32 ParentIdx = INDEX_NONE;
			if (OrigIdx == PrimaryRoot)
			{
				ParentIdx = INDEX_NONE;
			}
			else if (IsExtraRoot.Contains(OrigIdx))
			{
				ParentIdx = 0;
			}
			else
			{
				int32 OrigParent = Bone.ParentBone;
				if (OrigParent >= 0 && OrigParent < NumBones)
				{
					ParentIdx = RemapTable[OrigParent];
				}
				else
				{
					ParentIdx = 0;
				}
			}

			// WoW model space -> UE local: (Y, X, Z) * SCALE
			FVector Pivot(
				Bone.PivotPoint.Y * FWowCoordinate::SCALE,
				Bone.PivotPoint.X * FWowCoordinate::SCALE,
				Bone.PivotPoint.Z * FWowCoordinate::SCALE
			);

			FVector ParentPivot = FVector::ZeroVector;
			if (ParentIdx != INDEX_NONE)
			{
				int32 ParentOrigIdx = SortedOrder[ParentIdx];
				const FM2Bone& ParentBone = Data.Bones[ParentOrigIdx];
				ParentPivot = FVector(
					ParentBone.PivotPoint.Y * FWowCoordinate::SCALE,
					ParentBone.PivotPoint.X * FWowCoordinate::SCALE,
					ParentBone.PivotPoint.Z * FWowCoordinate::SCALE
				);
			}

			FTransform BoneTransform(FQuat::Identity, Pivot - ParentPivot, FVector::OneVector);

			FMeshBoneInfo BoneInfo;
			BoneInfo.Name = BoneName;
			BoneInfo.ParentIndex = ParentIdx;
			BoneInfo.ExportName = BoneName.ToString();

			SkelMod.Add(BoneInfo, BoneTransform);
		}
	}

	UE_LOG(LogWowSkelMesh, Log, TEXT("Created skeleton '%s' with %d bones (%d roots, primary=%d)"),
		*ModelName, NumBones, Roots.Num(), PrimaryRoot);
	return Skeleton;
}

USkeletalMesh* FWowSkeletalMeshBuilder::CreateSkeletalMesh(const FM2Data& Data, USkeleton* Skeleton,
	const FString& ModelName, FMpqManager* Mpq, FWowAssetCache* Cache)
{
	if (!Skeleton || Data.Vertices.Num() == 0 || Data.Indices.Num() == 0)
	{
		return nullptr;
	}

	const int32 NumVerts = Data.Vertices.Num();
	const int32 NumTris = Data.Indices.Num() / 3;
	const int32 NumBones = Data.Bones.Num();

	// Rebuild the same bone remap table used in CreateSkeleton (M2 index → sorted index)
	TArray<int32> BoneRemapTable;
	BoneRemapTable.SetNumUninitialized(NumBones);
	{
		TArray<int32> SortedOrder;
		SortedOrder.Reserve(NumBones);
		TArray<TArray<int32>> BoneChildren;
		BoneChildren.SetNum(NumBones);
		TArray<int32> BoneRoots;

		for (int32 i = 0; i < NumBones; i++)
		{
			int32 Parent = Data.Bones[i].ParentBone;
			if (Parent >= 0 && Parent < NumBones && Parent != i)
				BoneChildren[Parent].Add(i);
			else
				BoneRoots.Add(i);
		}

		// Reparent extra roots under primary root (same logic as CreateSkeleton)
		int32 PrimaryRoot = BoneRoots.Num() > 0 ? BoneRoots[0] : 0;
		for (int32 r = 1; r < BoneRoots.Num(); r++)
			BoneChildren[PrimaryRoot].Add(BoneRoots[r]);

		SortedOrder.Add(PrimaryRoot);
		int32 BfsHead = 0;
		while (BfsHead < SortedOrder.Num())
		{
			int32 Idx = SortedOrder[BfsHead++];
			for (int32 Child : BoneChildren[Idx])
				SortedOrder.Add(Child);
		}

		if (SortedOrder.Num() < NumBones)
		{
			TSet<int32> Added(SortedOrder);
			for (int32 i = 0; i < NumBones; i++)
				if (!Added.Contains(i))
					SortedOrder.Add(i);
		}

		for (int32 NewIdx = 0; NewIdx < NumBones; NewIdx++)
			BoneRemapTable[SortedOrder[NewIdx]] = NewIdx;
	}

	USkeletalMesh* SkelMesh = NewObject<USkeletalMesh>();
	SkelMesh->SetSkeleton(Skeleton);
	SkelMesh->SetRefSkeleton(Skeleton->GetReferenceSkeleton());

	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();

	// LOD info
	FSkeletalMeshLODInfo& LODInfo = SkelMesh->AddLODInfo();
	LODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
	LODInfo.ReductionSettings.NumOfVertPercentage = 1.0f;

	// Build import data arrays for IMeshUtilities::BuildSkeletalMesh
	TArray<FVector3f> Points;
	Points.SetNum(NumVerts);
	for (int32 i = 0; i < NumVerts; ++i)
	{
		const FM2Vertex& V = Data.Vertices[i];
		Points[i] = FVector3f(
			V.Position.Y * FWowCoordinate::SCALE,
			V.Position.X * FWowCoordinate::SCALE,
			V.Position.Z * FWowCoordinate::SCALE
		);
	}

	TArray<SkeletalMeshImportData::FMeshWedge> Wedges;
	Wedges.SetNum(Data.Indices.Num());
	for (int32 i = 0; i < Data.Indices.Num(); ++i)
	{
		int32 VertIdx = Data.Indices[i];
		Wedges[i].iVertex = VertIdx;
		Wedges[i].UVs[0] = FVector2f(Data.Vertices[VertIdx].TexCoord.X, Data.Vertices[VertIdx].TexCoord.Y);
		for (int32 u = 1; u < MAX_TEXCOORDS; ++u)
			Wedges[i].UVs[u] = FVector2f::ZeroVector;
		Wedges[i].Color = FColor::White;
	}

	TArray<SkeletalMeshImportData::FMeshFace> Faces;
	Faces.SetNum(NumTris);
	for (int32 i = 0; i < NumTris; ++i)
	{
		Faces[i].iWedge[0] = i * 3;
		Faces[i].iWedge[1] = i * 3 + 1;
		Faces[i].iWedge[2] = i * 3 + 2;
		Faces[i].MeshMaterialIndex = 0;
		Faces[i].SmoothingGroups = 1;

		for (int32 v = 0; v < 3; ++v)
		{
			int32 VertIdx = Data.Indices[i * 3 + v];
			const FM2Vertex& Vert = Data.Vertices[VertIdx];
			FVector3f Normal(Vert.Normal.Y, Vert.Normal.X, Vert.Normal.Z);
			Normal.Normalize();
			Faces[i].TangentZ[v] = Normal;
			FVector3f Tangent = FVector3f::CrossProduct(Normal, FVector3f(0, 1, 0));
			if (Tangent.SizeSquared() < 0.001f)
				Tangent = FVector3f::CrossProduct(Normal, FVector3f(1, 0, 0));
			Tangent.Normalize();
			Faces[i].TangentX[v] = Tangent;
			Faces[i].TangentY[v] = FVector3f::CrossProduct(Normal, Tangent);
		}
	}

	TArray<SkeletalMeshImportData::FVertInfluence> Influences;
	for (int32 i = 0; i < NumVerts; ++i)
	{
		const FM2Vertex& V = Data.Vertices[i];
		for (int32 j = 0; j < 4; ++j)
		{
			if (V.BoneWeights[j] > 0)
			{
				SkeletalMeshImportData::FVertInfluence Inf;
				Inf.VertIndex = i;
				int32 OrigBoneIdx = V.BoneIndices[j];
				Inf.BoneIndex = (OrigBoneIdx >= 0 && OrigBoneIdx < NumBones) ? BoneRemapTable[OrigBoneIdx] : 0;
				Inf.Weight = V.BoneWeights[j] / 255.0f;
				Influences.Add(Inf);
			}
		}
	}

	TArray<int32> PointToOriginalMap;
	PointToOriginalMap.SetNum(NumVerts);
	for (int32 i = 0; i < NumVerts; ++i)
		PointToOriginalMap[i] = i;

	// Build the LOD model using MeshUtilities
	IMeshUtilities& MeshUtils = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	FSkeletalMeshLODModel* LODModel = new FSkeletalMeshLODModel();

	IMeshUtilities::MeshBuildOptions BuildOptions;
	if (!MeshUtils.BuildSkeletalMesh(*LODModel, ModelName, RefSkel, Influences, Wedges, Faces, Points, PointToOriginalMap, BuildOptions))
	{
		UE_LOG(LogWowSkelMesh, Error, TEXT("BuildSkeletalMesh failed for '%s'"), *ModelName);
		delete LODModel;
		return nullptr;
	}

	// Set the LOD model on the imported model
	FSkeletalMeshModel* ImportedModel = SkelMesh->GetImportedModel();
	if (ImportedModel)
	{
		ImportedModel->LODModels.Empty();
		ImportedModel->LODModels.Add(LODModel);
	}

	// Apply material with first texture
	if (Data.TexturePaths.Num() > 0 && Mpq && Cache)
	{
		const FString& TexPath = Data.TexturePaths[0];
		if (!TexPath.IsEmpty())
		{
			UTexture2D* Tex = Cache->FindTexture(TexPath);
			if (!Tex)
			{
				TArray<uint8> BlpRaw;
				if (Mpq->ReadFile(TexPath, BlpRaw))
				{
					FBlpTexture BlpData = FBlpParser::Parse(BlpRaw);
					if (BlpData.bIsValid)
					{
						Tex = FWowTextureFactory::CreateTexture(BlpData, TexPath);
						if (Tex) Cache->CacheTexture(TexPath, Tex);
					}
				}
			}

			if (Tex)
			{
				UMaterial* BaseMat = UMaterial::GetDefaultMaterial(MD_Surface);
				if (BaseMat)
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, SkelMesh);
					MID->SetTextureParameterValue(FName(TEXT("BaseColor")), Tex);
					SkelMesh->GetMaterials().Add(FSkeletalMaterial(MID, true, false, FName(TEXT("M2_Material"))));
				}
			}
		}
	}

	if (SkelMesh->GetMaterials().Num() == 0)
	{
		SkelMesh->GetMaterials().Add(FSkeletalMaterial(UMaterial::GetDefaultMaterial(MD_Surface), true, false, FName(TEXT("Default"))));
	}

	// Set bounds
	FVector BoxMin = FVector(Data.BoundingBox.Min) * FWowCoordinate::SCALE;
	FVector BoxMax = FVector(Data.BoundingBox.Max) * FWowCoordinate::SCALE;
	FBoxSphereBounds Bounds(FBox(
		FVector(-BoxMax.X, BoxMin.Y, BoxMin.Z),
		FVector(-BoxMin.X, BoxMax.Y, BoxMax.Z)
	));
	SkelMesh->SetImportedBounds(Bounds);
	SkelMesh->CalculateInvRefMatrices();

	// Build render data directly from the LOD model
	SkelMesh->AllocateResourceForRendering();
	FSkeletalMeshRenderData* RenderData = SkelMesh->GetResourceForRendering();
	if (RenderData && LODModel)
	{
		RenderData->LODRenderData.Add(new FSkeletalMeshLODRenderData());
		FSkeletalMeshLODRenderData& LODRenderData = RenderData->LODRenderData[0];
		LODRenderData.BuildFromLODModel(LODModel, {});

		// Ensure color vertex buffer is initialized (vertex factory requires it)
		const int32 RenderVertCount = LODRenderData.GetNumVertices();
		if (LODRenderData.StaticVertexBuffers.ColorVertexBuffer.GetNumVertices() == 0 && RenderVertCount > 0)
		{
			LODRenderData.StaticVertexBuffers.ColorVertexBuffer.InitFromSingleColor(FColor::White, RenderVertCount);
		}

		RenderData->InitResources(true, SkelMesh);
		FlushRenderingCommands();
	}
	else
	{
		UE_LOG(LogWowSkelMesh, Error, TEXT("  Failed to create render data"));
	}

	UE_LOG(LogWowSkelMesh, Log, TEXT("Created skeletal mesh '%s' with %d verts, %d tris, %d bones"),
		*ModelName, NumVerts, NumTris, NumBones);

	return SkelMesh;
}

TArray<UAnimSequence*> FWowSkeletalMeshBuilder::CreateAnimations(const FM2Data& Data,
	USkeleton* Skeleton, const FString& ModelName)
{
	TArray<UAnimSequence*> Result;

	if (!Skeleton || !Data.HasAnimationData())
	{
		return Result;
	}

	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
	const int32 NumBones = RefSkel.GetNum();

	for (const FM2AnimationData& AnimData : Data.AnimationTracks)
	{
		if (AnimData.Duration == 0 || AnimData.BoneTracks.Num() == 0)
		{
			continue;
		}

		FString AnimName = FString::Printf(TEXT("%s_Anim%d_%d"), *ModelName, AnimData.AnimationId, AnimData.SubAnimationId);

		UAnimSequence* AnimSeq = NewObject<UAnimSequence>();
		AnimSeq->SetSkeleton(Skeleton);

		const float DurationSec = AnimData.Duration / 1000.0f;

		IAnimationDataController& Controller = AnimSeq->GetController();
		Controller.OpenBracket(FText::FromString(TEXT("ImportM2Anim")));
		Controller.SetFrameRate(FFrameRate(30, 1));

		const int32 NumFrames = FMath::Max(1, FMath::CeilToInt32(DurationSec * 30.0f));
		Controller.SetNumberOfFrames(NumFrames);

		const int32 TrackCount = FMath::Min(AnimData.BoneTracks.Num(), NumBones);
		for (int32 BoneIdx = 0; BoneIdx < TrackCount; ++BoneIdx)
		{
			const FM2BoneTrack& Track = AnimData.BoneTracks[BoneIdx];
			if (Track.IsEmpty())
			{
				continue;
			}

			FName BoneName = GetBoneName(BoneIdx);
			Controller.AddBoneCurve(BoneName);

			int32 MaxKeys = FMath::Max3(
				Track.TransTimestamps.Num(),
				Track.RotTimestamps.Num(),
				Track.ScaleTimestamps.Num()
			);
			if (MaxKeys == 0) MaxKeys = 1;

			TArray<FVector3f> PosKeys;
			TArray<FQuat4f> RotKeys;
			TArray<FVector3f> ScaleKeys;
			PosKeys.SetNum(MaxKeys);
			RotKeys.SetNum(MaxKeys);
			ScaleKeys.SetNum(MaxKeys);

			for (int32 k = 0; k < MaxKeys; ++k)
			{
				if (k < Track.TransValues.Num())
				{
					const FVector& T = Track.TransValues[k];
					PosKeys[k] = FVector3f(T.Y * FWowCoordinate::SCALE, T.X * FWowCoordinate::SCALE, T.Z * FWowCoordinate::SCALE);
				}
				else if (Track.TransValues.Num() > 0)
				{
					const FVector& T = Track.TransValues.Last();
					PosKeys[k] = FVector3f(T.Y * FWowCoordinate::SCALE, T.X * FWowCoordinate::SCALE, T.Z * FWowCoordinate::SCALE);
				}
				else
				{
					PosKeys[k] = FVector3f::ZeroVector;
				}

				if (k < Track.RotValues.Num())
				{
					const FQuat& R = Track.RotValues[k];
					RotKeys[k] = FQuat4f(R.Y, R.X, R.Z, -R.W);
				}
				else if (Track.RotValues.Num() > 0)
				{
					const FQuat& R = Track.RotValues.Last();
					RotKeys[k] = FQuat4f(R.Y, R.X, R.Z, -R.W);
				}
				else
				{
					RotKeys[k] = FQuat4f::Identity;
				}

				if (k < Track.ScaleValues.Num())
				{
					const FVector& S = Track.ScaleValues[k];
					ScaleKeys[k] = FVector3f(S.X, S.Y, S.Z);
				}
				else if (Track.ScaleValues.Num() > 0)
				{
					const FVector& S = Track.ScaleValues.Last();
					ScaleKeys[k] = FVector3f(S.X, S.Y, S.Z);
				}
				else
				{
					ScaleKeys[k] = FVector3f::OneVector;
				}
			}

			Controller.SetBoneTrackKeys(BoneName, PosKeys, RotKeys, ScaleKeys);
		}

		Controller.CloseBracket();
		Result.Add(AnimSeq);

		UE_LOG(LogWowSkelMesh, Log, TEXT("  Created animation '%s' (%.2fs, %s)"),
			*AnimName, DurationSec, AnimData.bIsLooping ? TEXT("loop") : TEXT("once"));
	}

	UE_LOG(LogWowSkelMesh, Log, TEXT("Created %d animations for '%s'"), Result.Num(), *ModelName);
	return Result;
}
