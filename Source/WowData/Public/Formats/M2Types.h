#pragma once
#include "CoreMinimal.h"

struct WOWDATA_API FM2Vertex
{
    FVector Position;
    FVector Normal;
    FVector2D TexCoord;
    FVector2D TexCoord2;
    uint8 BoneWeights[4] = {};
    uint8 BoneIndices[4] = {};
};

struct WOWDATA_API FM2RenderPass
{
    uint16 SubmeshIndex = 0;
    uint16 TextureIndex = 0;
    uint16 BlendMode = 0;
    uint16 Flags = 0;
};

struct WOWDATA_API FM2Bone
{
	int32 KeyBoneId = -1;
	uint32 Flags = 0;
	int16 ParentBone = -1;
	FVector PivotPoint = FVector::ZeroVector;
};

struct WOWDATA_API FM2AnimSequence
{
	uint16 AnimationId = 0;
	uint16 SubAnimationId = 0;
	uint32 Length = 0; // duration in ms
	float MoveSpeed = 0.0f;
	uint32 Flags = 0;
	int16 NextAnimation = -1;
	uint16 AliasNext = 0;
	bool IsLooping() const { return (Flags & 0x20) != 0; }
};

// Animation keyframe for a single bone
struct WOWDATA_API FM2AnimKeyframe
{
    uint32 Time = 0; // ms
    FVector Translation = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    FVector Scale = FVector::OneVector;
};

// Animation track for one bone across one animation sequence
struct WOWDATA_API FM2BoneTrack
{
    TArray<uint32> TransTimestamps;
    TArray<FVector> TransValues;
    TArray<uint32> RotTimestamps;
    TArray<FQuat> RotValues;
    TArray<uint32> ScaleTimestamps;
    TArray<FVector> ScaleValues;

    bool HasTranslation() const { return TransTimestamps.Num() > 0; }
    bool HasRotation() const { return RotTimestamps.Num() > 0; }
    bool HasScale() const { return ScaleTimestamps.Num() > 0; }
    bool IsEmpty() const { return !HasTranslation() && !HasRotation() && !HasScale(); }
};

// All animation tracks for all bones for one animation sequence
struct WOWDATA_API FM2AnimationData
{
    uint16 AnimationId = 0;
    uint16 SubAnimationId = 0;
    uint32 Duration = 0; // ms
    bool bIsLooping = false;
    float MoveSpeed = 0.0f;
    TArray<FM2BoneTrack> BoneTracks; // one per bone
};

struct WOWDATA_API FM2Data
{
    TArray<FM2Vertex> Vertices;
    TArray<uint16> Indices;
    TArray<FString> TexturePaths;
    TArray<FM2RenderPass> RenderPasses;
    FBox BoundingBox = FBox(ForceInit);
    float BoundingSphereRadius = 0.0f;
    TArray<FM2Bone> Bones;
    TArray<FM2AnimSequence> Animations;
    TArray<FM2AnimationData> AnimationTracks; // parsed keyframe data per animation
    uint32 NumBones = 0;
    bool bIsValid = false;

    bool HasBones() const { return Bones.Num() > 0; }
    bool HasAnimations() const { return Animations.Num() > 0; }
    bool HasAnimationData() const { return AnimationTracks.Num() > 0; }
};
