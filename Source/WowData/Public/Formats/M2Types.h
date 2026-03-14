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
    uint32 NumBones = 0;
    bool bIsValid = false;

    bool HasBones() const { return Bones.Num() > 0; }
    bool HasAnimations() const { return Animations.Num() > 0; }
};
