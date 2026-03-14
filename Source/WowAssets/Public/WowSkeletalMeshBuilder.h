#pragma once
#include "CoreMinimal.h"

struct FM2Data;
class USkeleton;
class USkeletalMesh;
class UAnimSequence;
class FMpqManager;
class FWowAssetCache;

/**
 * Builds USkeleton, USkeletalMesh, and UAnimSequence assets from parsed M2 data.
 */
class WOWASSETS_API FWowSkeletalMeshBuilder
{
public:
	/** Build a USkeleton from M2 bone hierarchy. Returns nullptr on failure. */
	static USkeleton* CreateSkeleton(const FM2Data& Data, const FString& ModelName);

	/** Build a USkeletalMesh from M2 vertices with bone weights. */
	static USkeletalMesh* CreateSkeletalMesh(const FM2Data& Data, USkeleton* Skeleton,
		const FString& ModelName, FMpqManager* Mpq, FWowAssetCache* Cache);

	/** Build UAnimSequence assets for all parsed animation tracks. */
	static TArray<UAnimSequence*> CreateAnimations(const FM2Data& Data, USkeleton* Skeleton,
		const FString& ModelName);

	/** Get a bone name for a given bone index (deterministic naming). */
	static FName GetBoneName(int32 BoneIndex);
};
