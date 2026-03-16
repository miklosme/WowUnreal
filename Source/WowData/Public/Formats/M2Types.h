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

struct WOWDATA_API FM2Attachment
{
    uint32 Id = 0;           // Attachment point ID (1=right palm, 2=left palm, 5=right shoulder, 11=helmet, etc.)
    int32 Bone = -1;         // Bone index this attachment is parented to
    FVector Position = FVector::ZeroVector; // Position relative to the bone
};

struct WOWDATA_API FM2ParticleEmitter
{
    uint32 Id = 0;                          // Particle emitter ID
    uint32 Flags = 0;                       // Emitter flags
    FVector Position = FVector::ZeroVector; // Position relative to bone
    int16 Bone = -1;                        // Bone index this emitter is attached to
    uint16 Texture = 0;                     // Texture index for particles
    FString BlendingType;                   // Blending mode for particles
    uint8 EmitterType = 0;                  // 1=plane, 2=sphere, 3=spline
    uint8 ParticleType = 0;                 // 0=regular, 1=chunky, 2=both
    uint8 HeadorTail = 0;                   // 0=head, 1=tail, 2=both
    int16 TailLength = 0;                   // Length of particle tail
    float MiddleTime = 0.0f;                // Time when particles are in middle phase
    uint32 ColorStart[3] = {255, 255, 255}; // RGB start color
    uint32 ColorMiddle[3] = {255, 255, 255}; // RGB middle color
    uint32 ColorEnd[3] = {255, 255, 255};   // RGB end color
    uint8 AlphaStart = 255;                 // Start alpha
    uint8 AlphaMiddle = 255;                // Middle alpha
    uint8 AlphaEnd = 255;                   // End alpha
    float ScaleStart = 1.0f;                // Start scale
    float ScaleMiddle = 1.0f;               // Middle scale
    float ScaleEnd = 1.0f;                  // End scale
    float ScaleVariation = 0.0f;            // Scale randomization
    float HeadLifeStart = 1000.0f;          // Head lifetime start (ms)
    float HeadLifeEnd = 1000.0f;            // Head lifetime end (ms)
    float HeadLifeRepeat = 0.0f;            // Head lifetime repeat rate
    float HeadDecay = 0.0f;                 // Head decay rate
    float TailLifeStart = 1000.0f;          // Tail lifetime start (ms)
    float TailLifeEnd = 1000.0f;            // Tail lifetime end (ms)
    float TailLifeRepeat = 0.0f;            // Tail lifetime repeat rate
    float TailDecay = 0.0f;                 // Tail decay rate
    float EmissionRate = 10.0f;             // Particles per second
    float EmissionAreaLength = 0.0f;        // Length of emission area
    float EmissionAreaWidth = 0.0f;         // Width of emission area
    float Gravity = -9.8f;                  // Gravity affecting particles

    // Helper functions
    bool IsFireEmitter() const;             // Check if this is a fire emitter
    bool IsSmokeEmitter() const;            // Check if this is a smoke emitter
    bool IsSparkleEmitter() const;          // Check if this is an ambient sparkle emitter
};

struct WOWDATA_API FM2Submesh
{
    uint16 Id = 0;           // Geoset ID (used for equipment geoset swaps)
    uint16 StartVertex = 0;
    uint16 NumVertices = 0;
    uint16 StartTriangle = 0;
    uint16 NumTriangles = 0;
    FVector CenterMass = FVector::ZeroVector;
    float Radius = 0.0f;
};

struct WOWDATA_API FM2Data
{
    TArray<FM2Vertex> Vertices;
    TArray<uint16> Indices;
    TArray<FString> TexturePaths;
    TArray<uint32> TextureTypes;  // M2 texture type per index (0=filename, 1=skin, 6=hair, etc.)
    TArray<FM2RenderPass> RenderPasses;
    FBox BoundingBox = FBox(ForceInit);
    float BoundingSphereRadius = 0.0f;
    TArray<FM2Bone> Bones;
    TArray<FM2AnimSequence> Animations;
    TArray<FM2AnimationData> AnimationTracks; // parsed keyframe data per animation
    TArray<FM2Attachment> Attachments;
    TArray<FM2ParticleEmitter> ParticleEmitters; // particle emitter data
    TArray<FM2Submesh> Submeshes;          // geoset info from skin file
    TArray<int16> AttachmentLookup;        // attachment ID → index in Attachments
    uint32 NumBones = 0;
    bool bIsValid = false;

    bool HasBones() const { return Bones.Num() > 0; }
    bool HasAnimations() const { return Animations.Num() > 0; }
    bool HasAnimationData() const { return AnimationTracks.Num() > 0; }
};
