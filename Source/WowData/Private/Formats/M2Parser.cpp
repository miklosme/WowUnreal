#include "Formats/M2Parser.h"
DEFINE_LOG_CATEGORY_STATIC(LogM2, Log, All);

// ── Raw on-disk structures ──────────────────────────────────────────────────

#pragma pack(push, 1)

struct FM2Header
{
    uint32 Magic;               // 'MD20' = 0x3032444D
    uint32 Version;             // 264 for WotLK
    uint32 NameLength;
    uint32 NameOffset;
    uint32 GlobalFlags;
    uint32 nGlobalSequences;
    uint32 ofsGlobalSequences;
    uint32 nAnimations;
    uint32 ofsAnimations;
    uint32 nAnimationLookup;
    uint32 ofsAnimationLookup;
    uint32 nBones;
    uint32 ofsBones;
    uint32 nKeyBoneLookup;
    uint32 ofsKeyBoneLookup;
    uint32 nVertices;
    uint32 ofsVertices;
    uint32 nViews;              // Always 0 in WotLK; skins are in .skin files
    uint32 nColors;
    uint32 ofsColors;
    uint32 nTextures;
    uint32 ofsTextures;
    uint32 nTransparency;
    uint32 ofsTransparency;
    uint32 nTexAnims;
    uint32 ofsTexAnims;
    uint32 nTexReplace;
    uint32 ofsTexReplace;
    uint32 nRenderFlags;
    uint32 ofsRenderFlags;
    uint32 nBoneLookup;
    uint32 ofsBoneLookup;
    uint32 nTexLookup;
    uint32 ofsTexLookup;
    uint32 nTexUnits;
    uint32 ofsTexUnits;
    uint32 nTransLookup;
    uint32 ofsTransLookup;
    uint32 nTexAnimLookup;
    uint32 ofsTexAnimLookup;
    float  BoundingBox[6];      // min xyz, max xyz
    float  BoundingSphereRadius;
    float  CollisionBox[6];
    float  CollisionSphereRadius;
    uint32 nBoundingTriangles;
    uint32 ofsBoundingTriangles;
    uint32 nBoundingVertices;
    uint32 ofsBoundingVertices;
    uint32 nBoundingNormals;
    uint32 ofsBoundingNormals;
    uint32 nAttachments;
    uint32 ofsAttachments;
    uint32 nAttachmentLookup;
    uint32 ofsAttachmentLookup;
    uint32 nEvents;
    uint32 ofsEvents;
    uint32 nLights;
    uint32 ofsLights;
    uint32 nCameras;
    uint32 ofsCameras;
    uint32 nCameraLookup;
    uint32 ofsCameraLookup;
    uint32 nRibbonEmitters;
    uint32 ofsRibbonEmitters;
    uint32 nParticleEmitters;
    uint32 ofsParticleEmitters;
};

struct FM2VertexRaw
{
    float  Pos[3];
    uint8  BoneWeights[4];
    uint8  BoneIndices[4];
    float  Normal[3];
    float  TexCoords[2];
    float  TexCoords2[2];
};

struct FM2TextureEntry
{
    uint32 Type;
    uint32 Flags;
    uint32 NameLength;
    uint32 NameOffset;
};

struct FM2RenderFlagEntry
{
    uint16 Flags;
    uint16 BlendMode;
};

// ── Skin file structures ────────────────────────────────────────────────────

struct FM2SkinHeader
{
    uint32 Magic;               // 'SKIN' = 0x4E494B53
    uint32 nIndices;
    uint32 ofsIndices;
    uint32 nTriangles;
    uint32 ofsTriangles;
    uint32 nProperties;
    uint32 ofsProperties;
    uint32 nSubmeshes;
    uint32 ofsSubmeshes;
    uint32 nTextureUnits;
    uint32 ofsTextureUnits;
    uint32 LOD;
};

struct FM2SkinSubmesh
{
    uint16 ID;
    uint16 Level;
    uint16 StartVertex;
    uint16 nVertices;
    uint16 StartTriangle;
    uint16 nTriangles;
    uint16 nBones;
    uint16 StartBones;
    uint16 BoneInfluences;
    uint16 RootBone;
    float  CenterMass[3];
    float  CenterBoundingBox[3];
    float  Radius;
};

struct FM2SkinTextureUnit
{
    uint8  Flags;
    int8   PriorityPlane;
    int16  ShaderId;
    uint16 SkinSectionIndex;
    uint16 GeosetIndex;
    int16  ColorIndex;
    uint16 RenderFlagsIndex;
    uint16 TexUnitNumber;
    uint16 OpCount;
    uint16 TextureComboIndex;
    uint16 TexCoordComboIndex;
    uint16 TransparencyComboIndex;
    uint16 TexAnimComboIndex;
};

struct FM2BoneRaw
{
	int32  KeyBoneId;
	uint32 Flags;
	int16  ParentBone;
	uint16 SubmeshId;
	uint16 Unk[2];
	// Animation blocks follow (translation, rotation, scale) — we skip for now
	// Each is 20 bytes (type, globalSeq, nTimestamps, ofsTimestamps, nValues, ofsValues)
	uint8 TransBlock[20];
	uint8 RotBlock[20];
	uint8 ScaleBlock[20];
	float PivotPoint[3];
};

struct FM2AnimSequenceRaw
{
	uint16 AnimationId;
	uint16 SubAnimationId;
	uint32 Length;
	float  MoveSpeed;
	uint32 Flags;
	int16  Probability;
	uint16 Padding;
	uint32 MinRepetitions;
	uint32 MaxRepetitions;
	uint32 BlendTime;
	float  BoundsMin[3];
	float  BoundsMax[3];
	float  BoundRadius;
	int16  NextAnimation;
	uint16 AliasNext;
};

struct FM2AttachmentRaw
{
    uint32 Id;
    uint32 Bone;
    float Position[3];
    uint8 EnabledBlock[20]; // AnimBlock — ignored
};

struct FM2ParticleEmitterRaw
{
    uint32 Id;
    uint32 Flags;
    float Position[3];
    int16 Bone;
    uint16 Texture;
    uint8 BlendingType[10]; // Model name for blending
    uint8 EmitterType;      // 1=plane, 2=sphere, 3=spline
    uint8 ParticleType;     // 0=regular, 1=chunky, 2=both
    uint8 HeadorTail;       // 0=head, 1=tail, 2=both
    int16 TailLength;
    float MiddleTime;
    uint32 ColorValues[3];   // AnimBlock for colors — we'll read the base values
    uint8 AlphaValues[3];    // AnimBlock for alpha — we'll read the base values
    uint32 ScaleValues[3];   // AnimBlock for scale — we'll read the base values
    uint8 ScaleVary[2];      // Scale variation
    uint32 HeadLife[2];      // AnimBlock for head lifetime
    uint32 HeadDecay[2];     // AnimBlock for head decay
    uint32 TailLife[2];      // AnimBlock for tail lifetime
    uint32 TailDecay[2];     // AnimBlock for tail decay
    uint8 Unknown[4];
    float Ref;
    uint32 Rows;
    uint32 Cols;
    uint32 EmissionRate[2];  // AnimBlock for emission rate
    uint32 SpeedVariation[2]; // AnimBlock for speed variation
    uint32 VerticalRange[2]; // AnimBlock for vertical range
    uint32 HorizontalRange[2]; // AnimBlock for horizontal range
    float Gravity;
    float Lifespan;
    uint32 Unknown2;
    uint32 EmissionRate2[2];
    uint32 EmissionAreaLength[2];
    uint32 EmissionAreaWidth[2];
    uint32 Gravity2[2];
};

#pragma pack(pop)

// ── Helpers ─────────────────────────────────────────────────────────────────

template<typename T>
static bool SafeRead(const uint8* Base, int32 TotalSize, uint32 Offset, uint32 Count, const T*& OutPtr)
{
    const uint64 End = static_cast<uint64>(Offset) + static_cast<uint64>(Count) * sizeof(T);
    if (End > static_cast<uint64>(TotalSize))
    {
        OutPtr = nullptr;
        return false;
    }
    OutPtr = reinterpret_cast<const T*>(Base + Offset);
    return true;
}

// ── Implementation ──────────────────────────────────────────────────────────

FM2Data FM2Parser::Parse(const TArray<uint8>& InData, const TArray<uint8>& SkinData)
{
    FM2Data Result;

    // ── Validate M2 header ──────────────────────────────────────────────
    if (InData.Num() < static_cast<int32>(sizeof(FM2Header)))
    {
        UE_LOG(LogM2, Error, TEXT("M2 data too small (%d bytes)"), InData.Num());
        return Result;
    }

    const uint8* M2Base = InData.GetData();
    const int32  M2Size = InData.Num();
    const FM2Header& Header = *reinterpret_cast<const FM2Header*>(M2Base);

    // Magic check: 'MD20' stored little-endian as 0x3032444D
    if (Header.Magic != 0x3032444D)
    {
        UE_LOG(LogM2, Error, TEXT("Bad M2 magic: 0x%08X"), Header.Magic);
        return Result;
    }

    UE_LOG(LogM2, Log, TEXT("M2 version=%u, verts=%u, textures=%u, bones=%u"),
        Header.Version, Header.nVertices, Header.nTextures, Header.nBones);

    // ── Read model name ─────────────────────────────────────────────────
    FString ModelName;
    if (Header.NameLength > 0 && Header.NameOffset + Header.NameLength <= static_cast<uint32>(M2Size))
    {
        ModelName = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(M2Base + Header.NameOffset)));
    }

    // ── Bounding box ────────────────────────────────────────────────────
    // WoW coordinate system: Z-up, Y-north. Convert to UE: X-forward, Z-up.
    // WoW (X,Y,Z) -> UE (Y, X, Z)  — or just store as-is and let the
    // mesh builder handle coordinate conversion.  We store raw here.
    Result.BoundingBox = FBox(
        FVector(Header.BoundingBox[0], Header.BoundingBox[1], Header.BoundingBox[2]),
        FVector(Header.BoundingBox[3], Header.BoundingBox[4], Header.BoundingBox[5])
    );
    Result.BoundingSphereRadius = Header.BoundingSphereRadius;
    Result.NumBones = Header.nBones;

    // ── Read vertices ───────────────────────────────────────────────────
    const FM2VertexRaw* RawVerts = nullptr;
    if (!SafeRead(M2Base, M2Size, Header.ofsVertices, Header.nVertices, RawVerts))
    {
        UE_LOG(LogM2, Error, TEXT("Vertex data out of bounds"));
        return Result;
    }

    Result.Vertices.SetNum(Header.nVertices);
    for (uint32 i = 0; i < Header.nVertices; i++)
    {
        const FM2VertexRaw& Src = RawVerts[i];
        FM2Vertex& Dst = Result.Vertices[i];
        Dst.Position = FVector(Src.Pos[0], Src.Pos[1], Src.Pos[2]);
        Dst.Normal   = FVector(Src.Normal[0], Src.Normal[1], Src.Normal[2]);
        Dst.TexCoord = FVector2D(Src.TexCoords[0], Src.TexCoords[1]);
        Dst.TexCoord2 = FVector2D(Src.TexCoords2[0], Src.TexCoords2[1]);
        FMemory::Memcpy(Dst.BoneWeights, Src.BoneWeights, 4);
        FMemory::Memcpy(Dst.BoneIndices, Src.BoneIndices, 4);
    }

    // ── Read bones ───────────────────────────────────────────────────────
    const FM2BoneRaw* RawBones = nullptr;
    if (Header.nBones > 0 && SafeRead(M2Base, M2Size, Header.ofsBones, Header.nBones, RawBones))
    {
        Result.Bones.SetNum(Header.nBones);
        for (uint32 i = 0; i < Header.nBones; i++)
        {
            const FM2BoneRaw& Src = RawBones[i];
            FM2Bone& Dst = Result.Bones[i];
            Dst.KeyBoneId = Src.KeyBoneId;
            Dst.Flags = Src.Flags;
            Dst.ParentBone = Src.ParentBone;
            Dst.PivotPoint = FVector(Src.PivotPoint[0], Src.PivotPoint[1], Src.PivotPoint[2]);
        }
        UE_LOG(LogM2, Log, TEXT("  Parsed %d bones"), Result.Bones.Num());
    }

    // ── Read animation sequences ────────────────────────────────────────
    const FM2AnimSequenceRaw* RawAnims = nullptr;
    if (Header.nAnimations > 0 && SafeRead(M2Base, M2Size, Header.ofsAnimations, Header.nAnimations, RawAnims))
    {
        Result.Animations.SetNum(Header.nAnimations);
        for (uint32 i = 0; i < Header.nAnimations; i++)
        {
            const FM2AnimSequenceRaw& Src = RawAnims[i];
            FM2AnimSequence& Dst = Result.Animations[i];
            Dst.AnimationId = Src.AnimationId;
            Dst.SubAnimationId = Src.SubAnimationId;
            Dst.Length = Src.Length;
            Dst.MoveSpeed = Src.MoveSpeed;
            Dst.Flags = Src.Flags;
            Dst.NextAnimation = Src.NextAnimation;
            Dst.AliasNext = Src.AliasNext;
        }
        UE_LOG(LogM2, Log, TEXT("  Parsed %d animation sequences"), Result.Animations.Num());
    }

    // ── Parse animation keyframes ─────────────────────────────────────────
    // Helper function to unpack quaternions from int16 values
    auto UnpackQuaternion = [](int16 x, int16 y, int16 z, int16 w) -> FQuat
    {
        return FQuat(
            (x < 0 ? (x + 32768) : (x - 32767)) / 32767.0f,
            (y < 0 ? (y + 32768) : (y - 32767)) / 32767.0f,
            (z < 0 ? (z + 32768) : (z - 32767)) / 32767.0f,
            (w < 0 ? (w + 32768) : (w - 32767)) / 32767.0f
        );
    };

    // Structure for animation block header (20 bytes)
    struct FAnimBlock
    {
        uint16 InterpolationType;
        uint16 GlobalSequence;
        uint32 nTimestampArrays;
        uint32 ofsTimestampArrays;
        uint32 nValueArrays;
        uint32 ofsValueArrays;
    };

    // Sub-array header (8 bytes)
    struct FSubArrayHeader
    {
        uint32 nEntries;
        uint32 ofsEntries;
    };

    if (Header.nAnimations > 0 && Header.nBones > 0 && RawBones != nullptr)
    {
        // Only parse keyframes for looping animations (internal data, not external .anim files)
        Result.AnimationTracks.Reserve(Header.nAnimations);

        for (uint32 animIdx = 0; animIdx < Header.nAnimations; animIdx++)
        {
            const FM2AnimSequenceRaw& AnimSeq = RawAnims[animIdx];

            // Skip animations that don't have embedded data (external .anim files)
            if ((AnimSeq.Flags & 0x20) == 0)
            {
                continue; // Not looping, likely external animation
            }

            FM2AnimationData AnimData;
            AnimData.AnimationId = AnimSeq.AnimationId;
            AnimData.SubAnimationId = AnimSeq.SubAnimationId;
            AnimData.Duration = AnimSeq.Length;
            AnimData.bIsLooping = (AnimSeq.Flags & 0x20) != 0;
            AnimData.MoveSpeed = AnimSeq.MoveSpeed;
            AnimData.BoneTracks.SetNum(Header.nBones);

            bool bHasAnyKeyframes = false;

            // Process each bone's animation data
            for (uint32 boneIdx = 0; boneIdx < Header.nBones; boneIdx++)
            {
                const FM2BoneRaw& Bone = RawBones[boneIdx];
                FM2BoneTrack& Track = AnimData.BoneTracks[boneIdx];

                // Parse translation block
                const FAnimBlock* TransBlock = reinterpret_cast<const FAnimBlock*>(Bone.TransBlock);
                if (TransBlock->nTimestampArrays > animIdx && TransBlock->nValueArrays > animIdx)
                {
                    const FSubArrayHeader* TimestampHeaders = nullptr;
                    const FSubArrayHeader* ValueHeaders = nullptr;

                    if (SafeRead(M2Base, M2Size, TransBlock->ofsTimestampArrays, TransBlock->nTimestampArrays, TimestampHeaders) &&
                        SafeRead(M2Base, M2Size, TransBlock->ofsValueArrays, TransBlock->nValueArrays, ValueHeaders))
                    {
                        const FSubArrayHeader& TimestampHeader = TimestampHeaders[animIdx];
                        const FSubArrayHeader& ValueHeader = ValueHeaders[animIdx];

                        if (TimestampHeader.nEntries > 0 && ValueHeader.nEntries > 0)
                        {
                            const uint32* Timestamps = nullptr;
                            const float* Values = nullptr; // 3 floats per entry (FVector)

                            if (SafeRead(M2Base, M2Size, TimestampHeader.ofsEntries, TimestampHeader.nEntries, Timestamps) &&
                                SafeRead(M2Base, M2Size, ValueHeader.ofsEntries, ValueHeader.nEntries * 3, Values))
                            {
                                Track.TransTimestamps.SetNum(TimestampHeader.nEntries);
                                Track.TransValues.SetNum(ValueHeader.nEntries);

                                FMemory::Memcpy(Track.TransTimestamps.GetData(), Timestamps, TimestampHeader.nEntries * sizeof(uint32));

                                for (uint32 i = 0; i < ValueHeader.nEntries; i++)
                                {
                                    Track.TransValues[i] = FVector(Values[i * 3], Values[i * 3 + 1], Values[i * 3 + 2]);
                                }
                                bHasAnyKeyframes = true;
                            }
                        }
                    }
                }

                // Parse rotation block
                const FAnimBlock* RotBlock = reinterpret_cast<const FAnimBlock*>(Bone.RotBlock);
                if (RotBlock->nTimestampArrays > animIdx && RotBlock->nValueArrays > animIdx)
                {
                    const FSubArrayHeader* TimestampHeaders = nullptr;
                    const FSubArrayHeader* ValueHeaders = nullptr;

                    if (SafeRead(M2Base, M2Size, RotBlock->ofsTimestampArrays, RotBlock->nTimestampArrays, TimestampHeaders) &&
                        SafeRead(M2Base, M2Size, RotBlock->ofsValueArrays, RotBlock->nValueArrays, ValueHeaders))
                    {
                        const FSubArrayHeader& TimestampHeader = TimestampHeaders[animIdx];
                        const FSubArrayHeader& ValueHeader = ValueHeaders[animIdx];

                        if (TimestampHeader.nEntries > 0 && ValueHeader.nEntries > 0)
                        {
                            const uint32* Timestamps = nullptr;
                            const int16* Values = nullptr; // 4 int16 per entry (packed quaternion)

                            if (SafeRead(M2Base, M2Size, TimestampHeader.ofsEntries, TimestampHeader.nEntries, Timestamps) &&
                                SafeRead(M2Base, M2Size, ValueHeader.ofsEntries, ValueHeader.nEntries * 4, Values))
                            {
                                Track.RotTimestamps.SetNum(TimestampHeader.nEntries);
                                Track.RotValues.SetNum(ValueHeader.nEntries);

                                FMemory::Memcpy(Track.RotTimestamps.GetData(), Timestamps, TimestampHeader.nEntries * sizeof(uint32));

                                for (uint32 i = 0; i < ValueHeader.nEntries; i++)
                                {
                                    int16 x = Values[i * 4];
                                    int16 y = Values[i * 4 + 1];
                                    int16 z = Values[i * 4 + 2];
                                    int16 w = Values[i * 4 + 3];
                                    Track.RotValues[i] = UnpackQuaternion(x, y, z, w);
                                }
                                bHasAnyKeyframes = true;
                            }
                        }
                    }
                }

                // Parse scale block
                const FAnimBlock* ScaleBlock = reinterpret_cast<const FAnimBlock*>(Bone.ScaleBlock);
                if (ScaleBlock->nTimestampArrays > animIdx && ScaleBlock->nValueArrays > animIdx)
                {
                    const FSubArrayHeader* TimestampHeaders = nullptr;
                    const FSubArrayHeader* ValueHeaders = nullptr;

                    if (SafeRead(M2Base, M2Size, ScaleBlock->ofsTimestampArrays, ScaleBlock->nTimestampArrays, TimestampHeaders) &&
                        SafeRead(M2Base, M2Size, ScaleBlock->ofsValueArrays, ScaleBlock->nValueArrays, ValueHeaders))
                    {
                        const FSubArrayHeader& TimestampHeader = TimestampHeaders[animIdx];
                        const FSubArrayHeader& ValueHeader = ValueHeaders[animIdx];

                        if (TimestampHeader.nEntries > 0 && ValueHeader.nEntries > 0)
                        {
                            const uint32* Timestamps = nullptr;
                            const float* Values = nullptr; // 3 floats per entry (FVector)

                            if (SafeRead(M2Base, M2Size, TimestampHeader.ofsEntries, TimestampHeader.nEntries, Timestamps) &&
                                SafeRead(M2Base, M2Size, ValueHeader.ofsEntries, ValueHeader.nEntries * 3, Values))
                            {
                                Track.ScaleTimestamps.SetNum(TimestampHeader.nEntries);
                                Track.ScaleValues.SetNum(ValueHeader.nEntries);

                                FMemory::Memcpy(Track.ScaleTimestamps.GetData(), Timestamps, TimestampHeader.nEntries * sizeof(uint32));

                                for (uint32 i = 0; i < ValueHeader.nEntries; i++)
                                {
                                    Track.ScaleValues[i] = FVector(Values[i * 3], Values[i * 3 + 1], Values[i * 3 + 2]);
                                }
                                bHasAnyKeyframes = true;
                            }
                        }
                    }
                }
            }

            if (bHasAnyKeyframes)
            {
                Result.AnimationTracks.Add(MoveTemp(AnimData));
            }
        }

        UE_LOG(LogM2, Log, TEXT("  Parsed %d animation tracks with keyframe data"), Result.AnimationTracks.Num());
    }

    // ── Read textures ───────────────────────────────────────────────────
    const FM2TextureEntry* TexEntries = nullptr;
    if (Header.nTextures > 0 && SafeRead(M2Base, M2Size, Header.ofsTextures, Header.nTextures, TexEntries))
    {
        Result.TexturePaths.SetNum(Header.nTextures);
        Result.TextureTypes.SetNum(Header.nTextures);
        for (uint32 i = 0; i < Header.nTextures; i++)
        {
            const FM2TextureEntry& Tex = TexEntries[i];
            Result.TextureTypes[i] = Tex.Type;
            if (Tex.Type == 0 && Tex.NameLength > 1 &&
                Tex.NameOffset + Tex.NameLength <= static_cast<uint32>(M2Size))
            {
                // Type 0 = filename-based texture
                FString Path = FString(UTF8_TO_TCHAR(
                    reinterpret_cast<const char*>(M2Base + Tex.NameOffset)));
                // Normalise path separators
                Path.ReplaceInline(TEXT("\\"), TEXT("/"));
                Result.TexturePaths[i] = Path;
            }
            else
            {
                // Type != 0 means a replaceable texture (skin, hair, etc.)
                // Store empty string; the renderer resolves these by type at runtime
                Result.TexturePaths[i] = FString();
            }
        }
    }

    // ── Read render flags ───────────────────────────────────────────────
    struct FRenderFlag { uint16 Flags; uint16 BlendMode; };
    TArray<FRenderFlag> RenderFlags;
    const FM2RenderFlagEntry* RFEntries = nullptr;
    if (Header.nRenderFlags > 0 && SafeRead(M2Base, M2Size, Header.ofsRenderFlags, Header.nRenderFlags, RFEntries))
    {
        RenderFlags.SetNum(Header.nRenderFlags);
        for (uint32 i = 0; i < Header.nRenderFlags; i++)
        {
            RenderFlags[i].Flags     = RFEntries[i].Flags;
            RenderFlags[i].BlendMode = RFEntries[i].BlendMode;
        }
    }

    // ── Read texture lookup table ───────────────────────────────────────
    TArray<uint16> TexLookup;
    const uint16* TexLookupPtr = nullptr;
    if (Header.nTexLookup > 0 && SafeRead(M2Base, M2Size, Header.ofsTexLookup, Header.nTexLookup, TexLookupPtr))
    {
        TexLookup.SetNum(Header.nTexLookup);
        FMemory::Memcpy(TexLookup.GetData(), TexLookupPtr, Header.nTexLookup * sizeof(uint16));
    }

    // ── Parse skin file ─────────────────────────────────────────────────
    if (SkinData.Num() < static_cast<int32>(sizeof(FM2SkinHeader)))
    {
        UE_LOG(LogM2, Error, TEXT("Skin data too small (%d bytes)"), SkinData.Num());
        return Result;
    }

    const uint8* SkinBase = SkinData.GetData();
    const int32  SkinSize = SkinData.Num();
    const FM2SkinHeader& Skin = *reinterpret_cast<const FM2SkinHeader*>(SkinBase);

    if (Skin.Magic != 0x4E494B53) // 'SKIN' in LE
    {
        UE_LOG(LogM2, Error, TEXT("Bad skin magic: 0x%08X"), Skin.Magic);
        return Result;
    }

    // ── Skin vertex indices (lookup into M2 vertex array) ───────────────
    const uint16* SkinVertexIndices = nullptr;
    if (!SafeRead(SkinBase, SkinSize, Skin.ofsIndices, Skin.nIndices, SkinVertexIndices))
    {
        UE_LOG(LogM2, Error, TEXT("Skin vertex indices out of bounds"));
        return Result;
    }

    // ── Skin triangle indices (index into SkinVertexIndices) ────────────
    const uint16* SkinTriangles = nullptr;
    if (!SafeRead(SkinBase, SkinSize, Skin.ofsTriangles, Skin.nTriangles, SkinTriangles))
    {
        UE_LOG(LogM2, Error, TEXT("Skin triangles out of bounds"));
        return Result;
    }

    // ── Submeshes ───────────────────────────────────────────────────────
    const FM2SkinSubmesh* SkinSubmeshes = nullptr;
    if (!SafeRead(SkinBase, SkinSize, Skin.ofsSubmeshes, Skin.nSubmeshes, SkinSubmeshes))
    {
        UE_LOG(LogM2, Error, TEXT("Skin submeshes out of bounds"));
        return Result;
    }

    // ── Texture units ───────────────────────────────────────────────────
    const FM2SkinTextureUnit* SkinTexUnits = nullptr;
    if (!SafeRead(SkinBase, SkinSize, Skin.ofsTextureUnits, Skin.nTextureUnits, SkinTexUnits))
    {
        UE_LOG(LogM2, Error, TEXT("Skin texture units out of bounds"));
        return Result;
    }

    // ── Build final index buffer ────────────────────────────────────────
    // The skin's triangle indices are indices into SkinVertexIndices[],
    // which are themselves indices into the M2's vertex array.
    // We resolve the double-indirection and emit direct vertex indices.
    Result.Indices.SetNum(Skin.nTriangles);
    for (uint32 i = 0; i < Skin.nTriangles; i++)
    {
        uint16 LocalIdx = SkinTriangles[i];
        if (LocalIdx < Skin.nIndices)
        {
            Result.Indices[i] = SkinVertexIndices[LocalIdx];
        }
        else
        {
            UE_LOG(LogM2, Warning, TEXT("Triangle index %u out of range (max %u)"), LocalIdx, Skin.nIndices);
            Result.Indices[i] = 0;
        }
    }

    // ── Build render passes from texture units ──────────────────────────
    Result.RenderPasses.SetNum(Skin.nTextureUnits);
    for (uint32 i = 0; i < Skin.nTextureUnits; i++)
    {
        const FM2SkinTextureUnit& TU = SkinTexUnits[i];
        FM2RenderPass& Pass = Result.RenderPasses[i];

        Pass.SubmeshIndex = TU.SkinSectionIndex;

        // Resolve texture index through the texture lookup table
        if (TU.TextureComboIndex < static_cast<uint16>(TexLookup.Num()))
        {
            Pass.TextureIndex = TexLookup[TU.TextureComboIndex];
        }
        else
        {
            Pass.TextureIndex = TU.TextureComboIndex;
        }

        // Resolve render flags
        if (TU.RenderFlagsIndex < static_cast<uint16>(RenderFlags.Num()))
        {
            Pass.Flags     = RenderFlags[TU.RenderFlagsIndex].Flags;
            Pass.BlendMode = RenderFlags[TU.RenderFlagsIndex].BlendMode;
        }
    }

    // ── Parse attachments ────────────────────────────────────────────────
    const FM2AttachmentRaw* RawAttachments = nullptr;
    if (Header.nAttachments > 0 && SafeRead(M2Base, M2Size, Header.ofsAttachments, Header.nAttachments, RawAttachments))
    {
        Result.Attachments.SetNum(Header.nAttachments);
        for (uint32 i = 0; i < Header.nAttachments; i++)
        {
            const FM2AttachmentRaw& Src = RawAttachments[i];
            FM2Attachment& Dst = Result.Attachments[i];
            Dst.Id = Src.Id;
            Dst.Bone = static_cast<int32>(Src.Bone);
            Dst.Position = FVector(Src.Position[0], Src.Position[1], Src.Position[2]);
        }
        UE_LOG(LogM2, Log, TEXT("  Parsed %d attachments"), Result.Attachments.Num());
    }

    // ── Parse attachment lookup ─────────────────────────────────────────
    const int16* RawAttachLookup = nullptr;
    if (Header.nAttachmentLookup > 0 && SafeRead(M2Base, M2Size, Header.ofsAttachmentLookup, Header.nAttachmentLookup, RawAttachLookup))
    {
        Result.AttachmentLookup.SetNum(Header.nAttachmentLookup);
        FMemory::Memcpy(Result.AttachmentLookup.GetData(), RawAttachLookup, Header.nAttachmentLookup * sizeof(int16));
    }

    // ── Parse particle emitters ─────────────────────────────────────────
    const FM2ParticleEmitterRaw* RawParticleEmitters = nullptr;
    if (Header.nParticleEmitters > 0 && SafeRead(M2Base, M2Size, Header.ofsParticleEmitters, Header.nParticleEmitters, RawParticleEmitters))
    {
        Result.ParticleEmitters.SetNum(Header.nParticleEmitters);
        for (uint32 i = 0; i < Header.nParticleEmitters; i++)
        {
            const FM2ParticleEmitterRaw& Src = RawParticleEmitters[i];
            FM2ParticleEmitter& Dst = Result.ParticleEmitters[i];

            Dst.Id = Src.Id;
            Dst.Flags = Src.Flags;
            Dst.Position = FVector(Src.Position[0], Src.Position[1], Src.Position[2]);
            Dst.Bone = Src.Bone;
            Dst.Texture = Src.Texture;
            Dst.BlendingType = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(Src.BlendingType)));
            Dst.EmitterType = Src.EmitterType;
            Dst.ParticleType = Src.ParticleType;
            Dst.HeadorTail = Src.HeadorTail;
            Dst.TailLength = Src.TailLength;
            Dst.MiddleTime = Src.MiddleTime;

            // Extract basic color values (ignoring animation blocks for simplicity)
            // In a real implementation, you'd want to parse the animation blocks
            // For now, we'll use reasonable defaults and try to extract base values
            Dst.ColorStart[0] = 255; Dst.ColorStart[1] = 128; Dst.ColorStart[2] = 64;  // Orange-ish default
            Dst.ColorMiddle[0] = 255; Dst.ColorMiddle[1] = 255; Dst.ColorMiddle[2] = 128; // Yellow-ish
            Dst.ColorEnd[0] = 128; Dst.ColorEnd[1] = 64; Dst.ColorEnd[2] = 32;   // Darker red

            Dst.AlphaStart = 255;
            Dst.AlphaMiddle = 192;
            Dst.AlphaEnd = 64;

            Dst.ScaleStart = 1.0f;
            Dst.ScaleMiddle = 1.5f;
            Dst.ScaleEnd = 2.0f;
            Dst.ScaleVariation = 0.5f;

            Dst.HeadLifeStart = 1000.0f; // 1 second default
            Dst.HeadLifeEnd = 2000.0f;   // 2 second default
            Dst.HeadLifeRepeat = 0.0f;
            Dst.HeadDecay = 0.1f;

            Dst.TailLifeStart = 500.0f;
            Dst.TailLifeEnd = 1000.0f;
            Dst.TailLifeRepeat = 0.0f;
            Dst.TailDecay = 0.2f;

            Dst.EmissionRate = 10.0f;      // 10 particles per second default
            Dst.EmissionAreaLength = 10.0f;
            Dst.EmissionAreaWidth = 10.0f;
            Dst.Gravity = Src.Gravity != 0.0f ? Src.Gravity : -9.8f;
        }
        UE_LOG(LogM2, Log, TEXT("  Parsed %d particle emitters"), Result.ParticleEmitters.Num());
    }

    // ── Parse submeshes from skin file ──────────────────────────────────
    if (SkinData.Num() >= static_cast<int32>(sizeof(FM2SkinHeader)))
    {
        const FM2SkinHeader* SkinHeader = reinterpret_cast<const FM2SkinHeader*>(SkinData.GetData());
        const FM2SkinSubmesh* RawSubmeshes = nullptr;
        if (SkinHeader->nSubmeshes > 0 && SafeRead(SkinData.GetData(), SkinData.Num(), SkinHeader->ofsSubmeshes, SkinHeader->nSubmeshes, RawSubmeshes))
        {
            Result.Submeshes.SetNum(SkinHeader->nSubmeshes);
            for (uint32 i = 0; i < SkinHeader->nSubmeshes; i++)
            {
                const FM2SkinSubmesh& Src = RawSubmeshes[i];
                FM2Submesh& Dst = Result.Submeshes[i];
                Dst.Id = Src.ID;
                Dst.StartVertex = Src.StartVertex;
                Dst.NumVertices = Src.nVertices;
                Dst.StartTriangle = Src.StartTriangle;
                Dst.NumTriangles = Src.nTriangles;
                Dst.CenterMass = FVector(Src.CenterMass[0], Src.CenterMass[1], Src.CenterMass[2]);
                Dst.Radius = Src.Radius;
            }
            UE_LOG(LogM2, Log, TEXT("  Parsed %d submeshes from skin"), Result.Submeshes.Num());
        }
    }

    Result.bIsValid = true;
    UE_LOG(LogM2, Log, TEXT("M2 '%s' parsed: %d verts, %d indices, %d passes, %d textures, %d attachments, %d submeshes, %d particle emitters"),
        *ModelName, Result.Vertices.Num(), Result.Indices.Num(),
        Result.RenderPasses.Num(), Result.TexturePaths.Num(),
        Result.Attachments.Num(), Result.Submeshes.Num(), Result.ParticleEmitters.Num());
    return Result;
}
