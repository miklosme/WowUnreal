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
#include "MeshDescription.h"
#include "SkeletalMeshAttributes.h"
#include "BoneWeights.h"

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

	FReferenceSkeleton RefSkel;
	FReferenceSkeletonModifier SkelMod(RefSkel, Skeleton);

	const int32 NumBones = Data.Bones.Num();

	for (int32 i = 0; i < NumBones; ++i)
	{
		const FM2Bone& Bone = Data.Bones[i];
		FName BoneName = GetBoneName(i);
		int32 ParentIdx = (Bone.ParentBone >= 0 && Bone.ParentBone < NumBones) ? Bone.ParentBone : INDEX_NONE;

		// WoW pivot in model space → relative to parent
		FVector Pivot(
			-Bone.PivotPoint.X * FWowCoordinate::SCALE,
			Bone.PivotPoint.Y * FWowCoordinate::SCALE,
			Bone.PivotPoint.Z * FWowCoordinate::SCALE
		);

		FVector ParentPivot = FVector::ZeroVector;
		if (ParentIdx != INDEX_NONE)
		{
			const FM2Bone& Parent = Data.Bones[ParentIdx];
			ParentPivot = FVector(
				-Parent.PivotPoint.X * FWowCoordinate::SCALE,
				Parent.PivotPoint.Y * FWowCoordinate::SCALE,
				Parent.PivotPoint.Z * FWowCoordinate::SCALE
			);
		}

		FTransform BoneTransform(FQuat::Identity, Pivot - ParentPivot, FVector::OneVector);

		FMeshBoneInfo BoneInfo;
		BoneInfo.Name = BoneName;
		BoneInfo.ParentIndex = ParentIdx;
		BoneInfo.ExportName = BoneName.ToString();

		SkelMod.Add(BoneInfo, BoneTransform);
	}

	UE_LOG(LogWowSkelMesh, Log, TEXT("Created skeleton '%s' with %d bones"), *ModelName, NumBones);
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

	USkeletalMesh* SkelMesh = NewObject<USkeletalMesh>();
	SkelMesh->SetSkeleton(Skeleton);
	SkelMesh->SetRefSkeleton(Skeleton->GetReferenceSkeleton());

	// LOD info
	FSkeletalMeshLODInfo& LODInfo = SkelMesh->AddLODInfo();
	LODInfo.ReductionSettings.NumOfTrianglesPercentage = 1.0f;
	LODInfo.ReductionSettings.NumOfVertPercentage = 1.0f;

	// Create mesh description at LOD 0
	FMeshDescription* MeshDescPtr = SkelMesh->CreateMeshDescription(0);
	if (!MeshDescPtr)
	{
		UE_LOG(LogWowSkelMesh, Error, TEXT("Failed to create mesh description for '%s'"), *ModelName);
		return nullptr;
	}
	FMeshDescription& MeshDesc = *MeshDescPtr;

	// Register skeletal mesh attributes (includes static mesh attrs + skin weights + bones)
	FSkeletalMeshAttributes SkelAttrs(MeshDesc);
	SkelAttrs.Register();

	FPolygonGroupID PolyGroup = MeshDesc.CreatePolygonGroup();
	MeshDesc.ReserveNewVertices(NumVerts);
	MeshDesc.ReserveNewVertexInstances(NumVerts);
	MeshDesc.ReserveNewPolygons(NumTris);
	MeshDesc.ReserveNewEdges(Data.Indices.Num());

	// Access standard vertex attributes via the FStaticMeshAttributes parent
	TVertexAttributesRef<FVector3f> VertexPositions = SkelAttrs.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> VertexNormals = SkelAttrs.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> VertexTangents = SkelAttrs.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> BinormalSigns = SkelAttrs.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> VertexUVs = SkelAttrs.GetVertexInstanceUVs();

	// Access skin weights
	FSkinWeightsVertexAttributesRef SkinWeightsRef = SkelAttrs.GetVertexSkinWeights();

	TArray<FVertexInstanceID> VertexInstanceIDs;
	VertexInstanceIDs.SetNum(NumVerts);

	for (int32 i = 0; i < NumVerts; ++i)
	{
		const FM2Vertex& V = Data.Vertices[i];
		FVertexID VertID = MeshDesc.CreateVertex();

		// Position: WoW RH → UE LH (negate X)
		VertexPositions[VertID] = FVector3f(
			-V.Position.X * FWowCoordinate::SCALE,
			V.Position.Y * FWowCoordinate::SCALE,
			V.Position.Z * FWowCoordinate::SCALE
		);

		// Set bone skin weights for this vertex
		{
			TArray<UE::AnimationCore::FBoneWeight> BoneWeightArray;
			for (int32 j = 0; j < 4; ++j)
			{
				if (V.BoneWeights[j] > 0)
				{
					BoneWeightArray.Add(UE::AnimationCore::FBoneWeight(
						static_cast<FBoneIndexType>(V.BoneIndices[j]),
						V.BoneWeights[j] / 255.0f
					));
				}
			}
			if (BoneWeightArray.Num() > 0)
			{
				UE::AnimationCore::FBoneWeights Weights = UE::AnimationCore::FBoneWeights::Create(BoneWeightArray);
				SkinWeightsRef.Set(VertID, Weights);
			}
		}

		FVertexInstanceID InstID = MeshDesc.CreateVertexInstance(VertID);
		VertexInstanceIDs[i] = InstID;

		FVector3f Normal(-V.Normal.X, V.Normal.Y, V.Normal.Z);
		Normal.Normalize();
		VertexNormals[InstID] = Normal;

		FVector3f T = FVector3f::CrossProduct(Normal, FVector3f(0, 1, 0));
		if (T.SizeSquared() < 0.001f)
		{
			T = FVector3f::CrossProduct(Normal, FVector3f(1, 0, 0));
		}
		T.Normalize();
		VertexTangents[InstID] = T;
		BinormalSigns[InstID] = 1.0f;
		VertexUVs.Set(InstID, 0, FVector2f(V.TexCoord.X, V.TexCoord.Y));
	}

	// Create triangles
	for (int32 i = 0; i < NumTris; ++i)
	{
		TArray<FVertexInstanceID> TriVerts;
		TriVerts.Add(VertexInstanceIDs[Data.Indices[i * 3]]);
		TriVerts.Add(VertexInstanceIDs[Data.Indices[i * 3 + 1]]);
		TriVerts.Add(VertexInstanceIDs[Data.Indices[i * 3 + 2]]);
		MeshDesc.CreatePolygon(PolyGroup, TriVerts);
	}

	// Register bones in mesh description
	SkelAttrs.ReserveNewBones(NumBones);
	auto BoneNames = SkelAttrs.GetBoneNames();
	auto BoneParentIndices = SkelAttrs.GetBoneParentIndices();
	auto BonePoses = SkelAttrs.GetBonePoses();

	const FReferenceSkeleton& RefSkel = Skeleton->GetReferenceSkeleton();
	for (int32 i = 0; i < NumBones; ++i)
	{
		FBoneID BoneID = SkelAttrs.CreateBone();
		BoneNames[BoneID] = GetBoneName(i);
		BoneParentIndices[BoneID] = (Data.Bones[i].ParentBone >= 0 && Data.Bones[i].ParentBone < NumBones)
			? Data.Bones[i].ParentBone : INDEX_NONE;
		BonePoses[BoneID] = RefSkel.GetRefBonePose()[i];
	}

	// Commit and build
	SkelMesh->CommitMeshDescription(0);
	SkelMesh->Build();
	SkelMesh->CalculateInvRefMatrices();

	// Set bounds
	FVector BoxMin = FVector(Data.BoundingBox.Min) * FWowCoordinate::SCALE;
	FVector BoxMax = FVector(Data.BoundingBox.Max) * FWowCoordinate::SCALE;
	FBoxSphereBounds Bounds(FBox(
		FVector(-BoxMax.X, BoxMin.Y, BoxMin.Z),
		FVector(-BoxMin.X, BoxMax.Y, BoxMax.Z)
	));
	SkelMesh->SetImportedBounds(Bounds);

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

	SkelMesh->InitResources();

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
					PosKeys[k] = FVector3f(-T.X * FWowCoordinate::SCALE, T.Y * FWowCoordinate::SCALE, T.Z * FWowCoordinate::SCALE);
				}
				else if (Track.TransValues.Num() > 0)
				{
					const FVector& T = Track.TransValues.Last();
					PosKeys[k] = FVector3f(-T.X * FWowCoordinate::SCALE, T.Y * FWowCoordinate::SCALE, T.Z * FWowCoordinate::SCALE);
				}
				else
				{
					PosKeys[k] = FVector3f::ZeroVector;
				}

				if (k < Track.RotValues.Num())
				{
					const FQuat& R = Track.RotValues[k];
					RotKeys[k] = FQuat4f(-R.X, R.Y, R.Z, R.W);
				}
				else if (Track.RotValues.Num() > 0)
				{
					const FQuat& R = Track.RotValues.Last();
					RotKeys[k] = FQuat4f(-R.X, R.Y, R.Z, R.W);
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
