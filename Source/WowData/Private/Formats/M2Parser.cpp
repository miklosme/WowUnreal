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

    // ── Read textures ───────────────────────────────────────────────────
    const FM2TextureEntry* TexEntries = nullptr;
    if (Header.nTextures > 0 && SafeRead(M2Base, M2Size, Header.ofsTextures, Header.nTextures, TexEntries))
    {
        Result.TexturePaths.SetNum(Header.nTextures);
        for (uint32 i = 0; i < Header.nTextures; i++)
        {
            const FM2TextureEntry& Tex = TexEntries[i];
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

    Result.bIsValid = true;
    UE_LOG(LogM2, Log, TEXT("M2 '%s' parsed: %d verts, %d indices, %d passes, %d textures"),
        *ModelName, Result.Vertices.Num(), Result.Indices.Num(),
        Result.RenderPasses.Num(), Result.TexturePaths.Num());
    return Result;
}
