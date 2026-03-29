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
#include "Animation/AnimData/IAnimationDataController.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "BoneWeights.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "IMeshBuilderModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"

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
			// UE 5.7: ExportName removed, Name is already an FName

			SkelMod.Add(BoneInfo, BoneTransform);
		}
	}

	UE_LOG(LogWowSkelMesh, Log, TEXT("Created skeleton '%s' with %d bones (%d roots, primary=%d)"),
		*ModelName, NumBones, Roots.Num(), PrimaryRoot);
	return Skeleton;
}

USkeletalMesh* FWowSkeletalMeshBuilder::CreateSkeletalMesh(const FM2Data& Data, USkeleton* Skeleton,
	const FString& ModelName, FMpqManager* Mpq, FWowAssetCache* Cache,
	TArray<FGeosetSectionInfo>* OutGeosetInfo,
	const TMap<uint16, uint16>* VisibleGeosets)
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

	// UE 5.7: CreateMeshDescription() removed, create FMeshDescription directly
	FMeshDescription MeshDesc;
	// Register skeletal mesh attributes
	FSkeletalMeshAttributes SkelAttrs(MeshDesc);
	SkelAttrs.Register();

	// Build list of render passes to include (filter by geoset visibility if provided).
	const int32 NumRenderPasses = Data.RenderPasses.Num();
	const bool bHasRenderPasses = NumRenderPasses > 0 && Data.Submeshes.Num() > 0;

	// Determine which render passes are visible
	TArray<int32> VisiblePasses;
	if (bHasRenderPasses)
	{
		for (int32 i = 0; i < NumRenderPasses; ++i)
		{
			const FM2RenderPass& Pass = Data.RenderPasses[i];
			if (Pass.SubmeshIndex >= static_cast<uint16>(Data.Submeshes.Num())) continue;

			bool bInclude = true;

			// Skip hair-textured passes (type 6) in the body mesh — hair gets its own mesh
			if (VisibleGeosets && Pass.TextureIndex < static_cast<uint16>(Data.TextureTypes.Num()) &&
				Data.TextureTypes[Pass.TextureIndex] == 6)
			{
				bInclude = false;
			}

			if (bInclude && VisibleGeosets)
			{
				uint16 GeosetId = Data.Submeshes[Pass.SubmeshIndex].Id;
				uint16 Group = GeosetId / 100;
				uint16 Variant = GeosetId % 100;

				const uint16* DesiredVariant = VisibleGeosets->Find(Group);
				if (DesiredVariant)
				{
					// GeosetId 0 = base body mesh, always include
					bInclude = (GeosetId == 0) || (Variant == *DesiredVariant);
				}
				// Groups not in rules: include by default (creatures, etc.)
			}
			if (bInclude)
			{
				VisiblePasses.Add(i);
			}
		}
	}
	else
	{
		VisiblePasses.Add(0); // Single section for models without render passes
	}

	const int32 NumSections = VisiblePasses.Num();
	TArray<FPolygonGroupID> PolyGroups;
	PolyGroups.SetNum(NumSections);
	for (int32 i = 0; i < NumSections; ++i)
	{
		PolyGroups[i] = MeshDesc.CreatePolygonGroup();
	}

	// Map from visible pass index to original render pass index
	// Output geoset info for each included section
	if (OutGeosetInfo)
	{
		OutGeosetInfo->SetNum(NumSections);
		for (int32 i = 0; i < NumSections; ++i)
		{
			FGeosetSectionInfo& Info = (*OutGeosetInfo)[i];
			Info.SectionIndex = i;
			if (bHasRenderPasses)
			{
				int32 OrigPassIdx = VisiblePasses[i];
				const FM2RenderPass& Pass = Data.RenderPasses[OrigPassIdx];
				if (Pass.SubmeshIndex < static_cast<uint16>(Data.Submeshes.Num()))
				{
					Info.GeosetId = Data.Submeshes[Pass.SubmeshIndex].Id;
					Info.GeosetGroup = Info.GeosetId / 100;
					Info.GeosetVariant = Info.GeosetId % 100;
					Info.TextureIndex = Pass.TextureIndex;
				}
			}
		}
	}

	MeshDesc.ReserveNewVertices(NumVerts);
	MeshDesc.ReserveNewVertexInstances(NumVerts);
	MeshDesc.ReserveNewPolygons(NumTris);
	MeshDesc.ReserveNewEdges(Data.Indices.Num());

	TVertexAttributesRef<FVector3f> VertexPositions = SkelAttrs.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexNormals = SkelAttrs.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VertexTangents = SkelAttrs.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> BinormalSigns = SkelAttrs.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> VertexUVs = SkelAttrs.GetVertexInstanceUVs();
	VertexUVs.SetNumChannels(1);

	FSkinWeightsVertexAttributesRef SkinWeightsRef = SkelAttrs.GetVertexSkinWeights();

	TArray<FVertexInstanceID> VertexInstanceIDs;
	VertexInstanceIDs.SetNum(NumVerts);

	for (int32 i = 0; i < NumVerts; ++i)
	{
		const FM2Vertex& V = Data.Vertices[i];
		FVertexID VertID = MeshDesc.CreateVertex();

		VertexPositions[VertID] = FVector3f(
			V.Position.Y * FWowCoordinate::SCALE,
			V.Position.X * FWowCoordinate::SCALE,
			V.Position.Z * FWowCoordinate::SCALE
		);

		TArray<UE::AnimationCore::FBoneWeight> BoneWeightArray;
		for (int32 j = 0; j < 4; ++j)
		{
			if (V.BoneWeights[j] <= 0)
			{
				continue;
			}

			const int32 OrigBoneIdx = V.BoneIndices[j];
			const int32 BoneIndex = (OrigBoneIdx >= 0 && OrigBoneIdx < NumBones) ? BoneRemapTable[OrigBoneIdx] : 0;
			BoneWeightArray.Add(UE::AnimationCore::FBoneWeight(static_cast<FBoneIndexType>(BoneIndex), V.BoneWeights[j] / 255.0f));
		}

		if (BoneWeightArray.Num() == 0)
		{
			BoneWeightArray.Add(UE::AnimationCore::FBoneWeight(0, 1.0f));
		}

		SkinWeightsRef.Set(VertID, UE::AnimationCore::FBoneWeights::Create(BoneWeightArray));

		FVertexInstanceID InstID = MeshDesc.CreateVertexInstance(VertID);
		VertexInstanceIDs[i] = InstID;

		FVector3f Normal(V.Normal.Y, V.Normal.X, V.Normal.Z);
		Normal.Normalize();
		VertexNormals[InstID] = Normal;

		FVector3f Tangent = FVector3f::CrossProduct(Normal, FVector3f(0, 1, 0));
		if (Tangent.SizeSquared() < 0.001f)
		{
			Tangent = FVector3f::CrossProduct(Normal, FVector3f(1, 0, 0));
		}
		Tangent.Normalize();
		VertexTangents[InstID] = Tangent;
		BinormalSigns[InstID] = 1.0f;
		VertexUVs.Set(InstID, 0, FVector2f(V.TexCoord.X, V.TexCoord.Y));
	}

	// Build triangles per-section. Only include render passes that are in VisiblePasses.
	if (bHasRenderPasses)
	{
		for (int32 SectionIdx = 0; SectionIdx < VisiblePasses.Num(); ++SectionIdx)
		{
			int32 OrigPassIdx = VisiblePasses[SectionIdx];
			const FM2RenderPass& Pass = Data.RenderPasses[OrigPassIdx];
			if (Pass.SubmeshIndex >= static_cast<uint16>(Data.Submeshes.Num()))
			{
				continue;
			}

			const FM2Submesh& Submesh = Data.Submeshes[Pass.SubmeshIndex];
			const int32 TriStart = Submesh.StartTriangle / 3;
			const int32 TriCount = Submesh.NumTriangles / 3;

			for (int32 t = 0; t < TriCount; ++t)
			{
				const int32 TriIdx = TriStart + t;
				if (TriIdx * 3 + 2 >= Data.Indices.Num())
				{
					break;
				}

				TArray<FVertexInstanceID> TriVerts;
				TriVerts.Reserve(3);
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const int32 VertexIndex = Data.Indices[TriIdx * 3 + Corner];
					if (VertexInstanceIDs.IsValidIndex(VertexIndex))
					{
						TriVerts.Add(VertexInstanceIDs[VertexIndex]);
					}
				}

				if (TriVerts.Num() == 3)
				{
					MeshDesc.CreatePolygon(PolyGroups[SectionIdx], TriVerts);
				}
			}
		}
	}
	else
	{
		// Fallback: single polygon group for all triangles
		for (int32 i = 0; i < NumTris; ++i)
		{
			TArray<FVertexInstanceID> TriVerts;
			TriVerts.Reserve(3);
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 VertexIndex = Data.Indices[i * 3 + Corner];
				if (VertexInstanceIDs.IsValidIndex(VertexIndex))
				{
					TriVerts.Add(VertexInstanceIDs[VertexIndex]);
				}
			}

			if (TriVerts.Num() == 3)
			{
				MeshDesc.CreatePolygon(PolyGroups[0], TriVerts);
			}
		}
	}

	SkelAttrs.ReserveNewBones(RefSkel.GetNum());
	auto BoneNames = SkelAttrs.GetBoneNames();
	auto BoneParentIndices = SkelAttrs.GetBoneParentIndices();
	auto BonePoses = SkelAttrs.GetBonePoses();
	for (int32 BoneIndex = 0; BoneIndex < RefSkel.GetNum(); ++BoneIndex)
	{
		FBoneID BoneID = SkelAttrs.CreateBone();
		BoneNames[BoneID] = RefSkel.GetBoneName(BoneIndex);
		BoneParentIndices[BoneID] = RefSkel.GetParentIndex(BoneIndex);
		BonePoses[BoneID] = RefSkel.GetRefBonePose()[BoneIndex];
	}

	// Materials MUST be set BEFORE Build(). Each section needs a UNIQUE material instance
	// to prevent UE5 from merging sections during Build() (which breaks per-section overrides).
	{
		UMaterial* DefaultMat = UMaterial::GetDefaultMaterial(MD_Surface);
		SkelMesh->GetMaterials().Reset();
		for (int32 i = 0; i < NumSections; ++i)
		{
			FName SlotName = FName(*FString::Printf(TEXT("Section_%d"), i));
			// Create unique MID per section so Build() keeps them separate
			UMaterialInstanceDynamic* UniqueMat = UMaterialInstanceDynamic::Create(DefaultMat, SkelMesh,
				FName(*FString::Printf(TEXT("SkelMat_%d"), i)));
			SkelMesh->GetMaterials().Add(FSkeletalMaterial(UniqueMat, true, false, SlotName));
		}
	}

	// UE 5.7: Use mesh description and MeshBuilderModule instead of the removed Build() pipeline
	// Configure build settings to preserve our M2 per-vertex normals (smooth shading)
	if (FSkeletalMeshLODInfo* LOD0Info = SkelMesh->GetLODInfo(0))
	{
		LOD0Info->BuildSettings.bRecomputeNormals = false;
		LOD0Info->BuildSettings.bRecomputeTangents = true;
		LOD0Info->BuildSettings.bUseMikkTSpace = false;
	}

	// Commit the mesh description for LOD 0
	SkelMesh->CommitMeshDescription(0);

	// Set bounds BEFORE Build() so the build pipeline has valid bounds data.
	// M2 vertex transform convention: UE = (WoW.Y, WoW.X, WoW.Z) * SCALE
	const FVector& BMin = Data.BoundingBox.Min;
	const FVector& BMax = Data.BoundingBox.Max;
	FBox MeshBox(
		FVector(FMath::Min(BMin.Y, BMax.Y) * FWowCoordinate::SCALE,
				FMath::Min(BMin.X, BMax.X) * FWowCoordinate::SCALE,
				FMath::Min(BMin.Z, BMax.Z) * FWowCoordinate::SCALE),
		FVector(FMath::Max(BMin.Y, BMax.Y) * FWowCoordinate::SCALE,
				FMath::Max(BMin.X, BMax.X) * FWowCoordinate::SCALE,
				FMath::Max(BMin.Z, BMax.Z) * FWowCoordinate::SCALE)
	);
	FBoxSphereBounds Bounds(MeshBox);
	SkelMesh->SetImportedBounds(Bounds);

	// CalculateInvRefMatrices must run before Build() so the build has valid inverse reference matrices.
	SkelMesh->CalculateInvRefMatrices();

	// UE 5.7: Use new render data pipeline with MeshBuilderModule
	// AllocateResourceForRendering creates FSkeletalMeshRenderData
	SkelMesh->AllocateResourceForRendering();

	// Get the render data and build it using the MeshBuilderModule
	FSkeletalMeshRenderData* RenderData = SkelMesh->GetResourceForRendering();
	if (RenderData)
	{
		// Cache derived data for the current platform
		ITargetPlatform* RunningPlatform = GetTargetPlatformManagerRef().GetRunningTargetPlatform();
		check(RunningPlatform);

		// Build parameters for the mesh builder
		FSkeletalMeshBuildParameters BuildParams(SkelMesh, RunningPlatform, 0, false);

		// Use MeshBuilderModule to build the render data from the mesh description
		IMeshBuilderModule& MeshBuilderModule = IMeshBuilderModule::GetForPlatform(RunningPlatform);
		if (!MeshBuilderModule.BuildSkeletalMesh(*RenderData, BuildParams))
		{
			UE_LOG(LogWowSkelMesh, Error, TEXT("Failed to build skeletal mesh render data for '%s'"), *ModelName);
			return nullptr;
		}

		// Initialize rendering resources
		RenderData->InitResources(false, SkelMesh);
	}
	else
	{
		UE_LOG(LogWowSkelMesh, Error, TEXT("Failed to allocate render data for skeletal mesh '%s'"), *ModelName);
		return nullptr;
	}

	UE_LOG(LogWowSkelMesh, Log, TEXT("Created skeletal mesh '%s' with %d verts, %d tris, %d bones"),
		*ModelName, NumVerts, NumTris, NumBones);

	return SkelMesh;
}

USkeletalMesh* FWowSkeletalMeshBuilder::CreateSkeletalMeshByTextureType(const FM2Data& Data,
	USkeleton* Skeleton, const FString& ModelName, FMpqManager* Mpq, FWowAssetCache* Cache,
	uint32 TextureType, const TMap<uint16, uint16>* VisibleGeosets)
{
	// Build a filter that only includes render passes matching the requested texture type
	// AND matching geoset visibility rules
	TMap<uint16, uint16> TextureTypeFilter;

	// We pass a special VisibleGeosets that allows ALL geosets but we filter render passes
	// by texture type in a custom pass filter
	// Actually, let's reuse CreateSkeletalMesh with a custom approach:
	// Create a modified M2Data that only has render passes for the desired texture type

	FM2Data FilteredData = Data; // Copy
	FilteredData.RenderPasses.Reset();

	for (const FM2RenderPass& Pass : Data.RenderPasses)
	{
		// Only include passes that reference the desired texture type
		if (Pass.TextureIndex < static_cast<uint16>(Data.TextureTypes.Num()) &&
			Data.TextureTypes[Pass.TextureIndex] == TextureType)
		{
			// Also check geoset visibility
			if (VisibleGeosets && Pass.SubmeshIndex < static_cast<uint16>(Data.Submeshes.Num()))
			{
				uint16 GeosetId = Data.Submeshes[Pass.SubmeshIndex].Id;
				uint16 Group = GeosetId / 100;
				uint16 Variant = GeosetId % 100;
				const uint16* DesiredVariant = VisibleGeosets->Find(Group);
				if (DesiredVariant)
				{
					if (GeosetId != 0 && Variant != *DesiredVariant) continue;
				}
			}
			FilteredData.RenderPasses.Add(Pass);
		}
	}

	if (FilteredData.RenderPasses.Num() == 0)
	{
		return nullptr;
	}

	return CreateSkeletalMesh(FilteredData, Skeleton, ModelName, Mpq, Cache, nullptr, nullptr);
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

#if WITH_EDITOR
		IAnimationDataController& Controller = AnimSeq->GetController();
		Controller.InitializeModel();
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
			const int32 RefBoneIndex = RefSkel.FindBoneIndex(BoneName);
			const FTransform BaseTransform = RefBoneIndex != INDEX_NONE ? RefSkel.GetRefBonePose()[RefBoneIndex] : FTransform::Identity;
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
				PosKeys[k] = FVector3f(BaseTransform.GetTranslation());
				RotKeys[k] = FQuat4f(BaseTransform.GetRotation());
				ScaleKeys[k] = FVector3f(BaseTransform.GetScale3D());

				if (k < Track.TransValues.Num())
				{
					const FVector& T = Track.TransValues[k];
					PosKeys[k] += FVector3f(T.Y * FWowCoordinate::SCALE, T.X * FWowCoordinate::SCALE, T.Z * FWowCoordinate::SCALE);
				}
				else if (Track.TransValues.Num() > 0)
				{
					const FVector& T = Track.TransValues.Last();
					PosKeys[k] += FVector3f(T.Y * FWowCoordinate::SCALE, T.X * FWowCoordinate::SCALE, T.Z * FWowCoordinate::SCALE);
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
			}

			Controller.SetBoneTrackKeys(BoneName, PosKeys, RotKeys, ScaleKeys);
		}

		Controller.CloseBracket();
		Controller.NotifyPopulated();
#else
		UE_LOG(LogWowSkelMesh, Warning, TEXT("  Skipping animation '%s' — GetController() requires editor"), *AnimName);
#endif // WITH_EDITOR
		Result.Add(AnimSeq);

		UE_LOG(LogWowSkelMesh, Log, TEXT("  Created animation '%s' (%.2fs, %s)"),
			*AnimName, DurationSec, AnimData.bIsLooping ? TEXT("loop") : TEXT("once"));
	}

	UE_LOG(LogWowSkelMesh, Log, TEXT("Created %d animations for '%s'"), Result.Num(), *ModelName);
	return Result;
}
