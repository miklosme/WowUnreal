#include "Formats/WmoParser.h"

DEFINE_LOG_CATEGORY_STATIC(LogWmo, Log, All);

namespace
{
// Helper to build a FourCC uint32 from a 4-char string literal.
// WoW chunk magics are stored reversed in the file, e.g. "MOHD" is stored as 'D','H','O','M'.
// We define the expected uint32 value as it appears when read as a little-endian uint32.
constexpr uint32 MakeFourCC(char A, char B, char C, char D)
{
    return static_cast<uint32>(A)
        | (static_cast<uint32>(B) << 8)
        | (static_cast<uint32>(C) << 16)
        | (static_cast<uint32>(D) << 24);
}

// Root chunks — REVERSED order, as stored on disk (little-endian uint32 read)
constexpr uint32 CHUNK_MVER = MakeFourCC('R','E','V','M');
constexpr uint32 CHUNK_MOHD = MakeFourCC('D','H','O','M');
constexpr uint32 CHUNK_MOTX = MakeFourCC('X','T','O','M');
constexpr uint32 CHUNK_MOMT = MakeFourCC('T','M','O','M');
constexpr uint32 CHUNK_MOGN = MakeFourCC('N','G','O','M');
constexpr uint32 CHUNK_MOGI = MakeFourCC('I','G','O','M');
constexpr uint32 CHUNK_MODS = MakeFourCC('S','D','O','M');
constexpr uint32 CHUNK_MODN = MakeFourCC('N','D','O','M');
constexpr uint32 CHUNK_MODD = MakeFourCC('D','D','O','M');

// Group chunks — REVERSED order, as stored on disk (little-endian uint32 read)
constexpr uint32 CHUNK_MOGP = MakeFourCC('P','G','O','M');
constexpr uint32 CHUNK_MOPY = MakeFourCC('Y','P','O','M');
constexpr uint32 CHUNK_MOVI = MakeFourCC('I','V','O','M');
constexpr uint32 CHUNK_MOVT = MakeFourCC('T','V','O','M');
constexpr uint32 CHUNK_MONR = MakeFourCC('R','N','O','M');
constexpr uint32 CHUNK_MOTV = MakeFourCC('V','T','O','M');
constexpr uint32 CHUNK_MOBA = MakeFourCC('A','B','O','M');
constexpr uint32 CHUNK_MOCV = MakeFourCC('V','C','O','M');

// Safe read helpers
    template<typename T>
    FORCEINLINE T ReadVal(const uint8* Base, uint32& Offset, uint32 DataSize)
    {
        if (Offset + sizeof(T) > DataSize)
        {
            Offset = DataSize; // prevent further reads
            return T{};
        }
        T Value;
        FMemory::Memcpy(&Value, Base + Offset, sizeof(T));
        Offset += sizeof(T);
        return Value;
    }

    FORCEINLINE float ReadFloat(const uint8* Base, uint32& Offset, uint32 DataSize)
    {
        return ReadVal<float>(Base, Offset, DataSize);
    }

    FORCEINLINE uint32 ReadU32(const uint8* Base, uint32& Offset, uint32 DataSize)
    {
        return ReadVal<uint32>(Base, Offset, DataSize);
    }

    FORCEINLINE uint16 ReadU16(const uint8* Base, uint32& Offset, uint32 DataSize)
    {
        return ReadVal<uint16>(Base, Offset, DataSize);
    }

    FORCEINLINE uint8 ReadU8(const uint8* Base, uint32& Offset, uint32 DataSize)
    {
        return ReadVal<uint8>(Base, Offset, DataSize);
    }

    FORCEINLINE int32 ReadI32(const uint8* Base, uint32& Offset, uint32 DataSize)
    {
        return ReadVal<int32>(Base, Offset, DataSize);
    }

    FORCEINLINE int16 ReadI16(const uint8* Base, uint32& Offset, uint32 DataSize)
    {
        return ReadVal<int16>(Base, Offset, DataSize);
    }

    // Read a null-terminated string from a string table at a given offset
    FString ReadStringFromTable(const uint8* TableBase, uint32 TableSize, uint32 StringOffset)
    {
        if (StringOffset >= TableSize)
        {
            return FString();
        }
        // Find the null terminator
        uint32 End = StringOffset;
        while (End < TableSize && TableBase[End] != 0)
        {
            End++;
        }
        if (End == StringOffset)
        {
            return FString();
        }
        // Convert to FString (ASCII)
        return FString(End - StringOffset, reinterpret_cast<const char*>(TableBase + StringOffset));
    }

    // Collect all null-terminated strings from a string table as (offset, string) pairs
    void CollectAllStrings(const uint8* TableBase, uint32 TableSize, TArray<TPair<uint32, FString>>& OutStrings)
    {
        uint32 Pos = 0;
        while (Pos < TableSize)
        {
            // Skip null padding bytes
            if (TableBase[Pos] == 0)
            {
                Pos++;
                continue;
            }
            uint32 Start = Pos;
            while (Pos < TableSize && TableBase[Pos] != 0)
            {
                Pos++;
            }
            FString Str(Pos - Start, reinterpret_cast<const char*>(TableBase + Start));
            OutStrings.Add(TPair<uint32, FString>(Start, MoveTemp(Str)));
            if (Pos < TableSize) Pos++; // skip the null terminator
        }
    }
}

// ============================================================================
// ParseRoot
// ============================================================================
FWmoRootData FWmoParser::ParseRoot(const TArray<uint8>& Data)
{
    FWmoRootData Result;
    const uint8* Base = Data.GetData();
    const uint32 DataSize = static_cast<uint32>(Data.Num());

    if (DataSize < 8)
    {
        UE_LOG(LogWmo, Warning, TEXT("WMO root data too small (%d bytes)"), DataSize);
        return Result;
    }

    // Texture name string table (populated from MOTX chunk)
    const uint8* TexNameTable = nullptr;
    uint32 TexNameTableSize = 0;

    // Doodad name string table (populated from MODN chunk)
    const uint8* DoodNameTable = nullptr;
    uint32 DoodNameTableSize = 0;

    // Iterate chunks
    uint32 Offset = 0;
    while (Offset + 8 <= DataSize)
    {
        const uint32 ChunkMagic = ReadU32(Base, Offset, DataSize);
        const uint32 ChunkSize = ReadU32(Base, Offset, DataSize);
        const uint32 ChunkStart = Offset;

        if (ChunkStart + ChunkSize > DataSize)
        {
            UE_LOG(LogWmo, Warning, TEXT("WMO root chunk 0x%08X at offset %u overflows (size=%u, remaining=%u)"),
                ChunkMagic, ChunkStart - 8, ChunkSize, DataSize - ChunkStart);
            break;
        }

        if (ChunkMagic == CHUNK_MOHD)
        {
            // Header: 64 bytes
            if (ChunkSize < 64)
            {
                UE_LOG(LogWmo, Warning, TEXT("MOHD chunk too small: %u"), ChunkSize);
                Offset = ChunkStart + ChunkSize;
                continue;
            }
            uint32 Off = ChunkStart;
            const uint32 nMaterials  = ReadU32(Base, Off, DataSize);
            const uint32 nGroups     = ReadU32(Base, Off, DataSize);
            /*nPortals*/               ReadU32(Base, Off, DataSize);
            /*nLights*/                ReadU32(Base, Off, DataSize);
            /*nModels*/                ReadU32(Base, Off, DataSize);
            /*nDoodads*/               ReadU32(Base, Off, DataSize);
            /*nSets*/                  ReadU32(Base, Off, DataSize);
            const uint32 AmbientBGRA = ReadU32(Base, Off, DataSize);
            /*wmoID*/                  ReadU32(Base, Off, DataSize);

            // Bounding box: min (3 floats), max (3 floats)
            const float MinX = ReadFloat(Base, Off, DataSize);
            const float MinY = ReadFloat(Base, Off, DataSize);
            const float MinZ = ReadFloat(Base, Off, DataSize);
            const float MaxX = ReadFloat(Base, Off, DataSize);
            const float MaxY = ReadFloat(Base, Off, DataSize);
            const float MaxZ = ReadFloat(Base, Off, DataSize);

            // WoW coord system: X right, Y up(?), Z forward  ->  UE: swap Y/Z, negate as needed
            // For WMO we store raw WoW coords; the rendering layer handles conversion.
            Result.BoundingBox = FBox(FVector(MinX, MinY, MinZ), FVector(MaxX, MaxY, MaxZ));

            const uint16 Flags  = ReadU16(Base, Off, DataSize);
            /*numLods*/           ReadU16(Base, Off, DataSize);

            Result.NumGroups = nGroups;
            Result.Flags = Flags;

            (void)nMaterials; // materials count comes from MOMT chunk size
            (void)AmbientBGRA;
        }
        else if (ChunkMagic == CHUNK_MOTX)
        {
            // Texture name string table - just store pointer, will be used by MOMT
            TexNameTable = Base + ChunkStart;
            TexNameTableSize = ChunkSize;
        }
        else if (ChunkMagic == CHUNK_MOMT)
        {
            // Materials: 64 bytes each
            const uint32 NumMaterials = ChunkSize / 64;
            Result.Materials.Reserve(NumMaterials);

            uint32 Off = ChunkStart;
            for (uint32 i = 0; i < NumMaterials; i++)
            {
                FWmoMaterial Mat;
                Mat.Flags     = ReadU32(Base, Off, DataSize);
                Mat.Shader    = ReadU32(Base, Off, DataSize);
                Mat.BlendMode = ReadU32(Base, Off, DataSize);

                const uint32 Tex1Ofs = ReadU32(Base, Off, DataSize);

                // Emissive color (BGRA in file)
                const uint8 EmB = ReadU8(Base, Off, DataSize);
                const uint8 EmG = ReadU8(Base, Off, DataSize);
                const uint8 EmR = ReadU8(Base, Off, DataSize);
                const uint8 EmA = ReadU8(Base, Off, DataSize);
                Mat.EmissiveColor = FColor(EmR, EmG, EmB, EmA);

                // sidn_emissive_color / flags2 (4 bytes, skip)
                Off += 4;

                const uint32 Tex2Ofs = ReadU32(Base, Off, DataSize);

                // Diffuse color (ARGB layout per CArgb: R,G,B,A)
                const uint8 DiffR = ReadU8(Base, Off, DataSize);
                const uint8 DiffG = ReadU8(Base, Off, DataSize);
                const uint8 DiffB = ReadU8(Base, Off, DataSize);
                const uint8 DiffA = ReadU8(Base, Off, DataSize);
                Mat.DiffuseColor = FColor(DiffR, DiffG, DiffB, DiffA);

                /*groundType*/  ReadU32(Base, Off, DataSize);
                const uint32 Tex3Ofs = ReadU32(Base, Off, DataSize);
                /*color2*/      ReadU32(Base, Off, DataSize);
                /*flags3*/      ReadU32(Base, Off, DataSize);
                // runtime data: 4 uint32s
                Off += 16;

                // Resolve texture paths from string table
                if (TexNameTable)
                {
                    Mat.TexturePath1 = ReadStringFromTable(TexNameTable, TexNameTableSize, Tex1Ofs);
                    Mat.TexturePath2 = ReadStringFromTable(TexNameTable, TexNameTableSize, Tex2Ofs);
                }

                (void)Tex3Ofs;

                Result.Materials.Add(MoveTemp(Mat));
            }
        }
        else if (ChunkMagic == CHUNK_MODN)
        {
            // Doodad name string table
            DoodNameTable = Base + ChunkStart;
            DoodNameTableSize = ChunkSize;

            // Collect all doodad paths
            TArray<TPair<uint32, FString>> AllStrings;
            CollectAllStrings(DoodNameTable, DoodNameTableSize, AllStrings);
            Result.DoodadPaths.Reserve(AllStrings.Num());
            for (auto& Pair : AllStrings)
            {
                Result.DoodadPaths.Add(MoveTemp(Pair.Value));
            }
        }

        // Advance to next chunk
        Offset = ChunkStart + ChunkSize;
    }

    Result.bIsValid = true;
    UE_LOG(LogWmo, Log, TEXT("WMO root parsed: %d materials, %d groups, %d doodad paths"),
        Result.Materials.Num(), Result.NumGroups, Result.DoodadPaths.Num());
    return Result;
}

// ============================================================================
// ParseGroup
// ============================================================================
FWmoGroupData FWmoParser::ParseGroup(const TArray<uint8>& Data)
{
    FWmoGroupData Result;
    const uint8* Base = Data.GetData();
    const uint32 DataSize = static_cast<uint32>(Data.Num());

    if (DataSize < 8)
    {
        UE_LOG(LogWmo, Warning, TEXT("WMO group data too small (%d bytes)"), DataSize);
        return Result;
    }

    // The group file starts with an MVER chunk, then the MOGP chunk which wraps all sub-chunks.
    // MOGP's chunk size covers the group header + all sub-chunks inside it.
    // We need to find MOGP first, parse its header, then iterate sub-chunks inside it.

    uint32 Offset = 0;
    uint32 MogpDataStart = 0;
    uint32 MogpDataEnd = 0;

    // Find the MOGP chunk
    while (Offset + 8 <= DataSize)
    {
        const uint32 ChunkMagic = ReadU32(Base, Offset, DataSize);
        const uint32 ChunkSize = ReadU32(Base, Offset, DataSize);
        const uint32 ChunkStart = Offset;

        if (ChunkStart + ChunkSize > DataSize)
        {
            break;
        }

        if (ChunkMagic == CHUNK_MOGP)
        {
            // Parse the MOGP header (68 bytes)
            if (ChunkSize < 68)
            {
                UE_LOG(LogWmo, Warning, TEXT("MOGP chunk too small: %u"), ChunkSize);
                return Result;
            }

            uint32 Off = ChunkStart;
            /*groupName*/         ReadU32(Base, Off, DataSize);
            /*descriptiveGroup*/  ReadU32(Base, Off, DataSize);
            Result.Flags        = ReadU32(Base, Off, DataSize);

            const float MinX = ReadFloat(Base, Off, DataSize);
            const float MinY = ReadFloat(Base, Off, DataSize);
            const float MinZ = ReadFloat(Base, Off, DataSize);
            const float MaxX = ReadFloat(Base, Off, DataSize);
            const float MaxY = ReadFloat(Base, Off, DataSize);
            const float MaxZ = ReadFloat(Base, Off, DataSize);
            Result.BoundingBox = FBox(FVector(MinX, MinY, MinZ), FVector(MaxX, MaxY, MaxZ));

            /*portalStart*/       ReadU16(Base, Off, DataSize);
            /*portalCount*/       ReadU16(Base, Off, DataSize);
            /*transBatchCount*/   ReadU16(Base, Off, DataSize);
            /*intBatchCount*/     ReadU16(Base, Off, DataSize);
            /*extBatchCount*/     ReadU16(Base, Off, DataSize);
            /*padding*/           ReadU16(Base, Off, DataSize);
            // fogs[4]
            Off += 4;
            /*groupLiquid*/       ReadU32(Base, Off, DataSize);
            /*groupID*/           ReadU32(Base, Off, DataSize);
            /*unk1*/              ReadU32(Base, Off, DataSize);
            /*unk2*/              ReadU32(Base, Off, DataSize);

            // Sub-chunks start right after the 68-byte MOGP header
            MogpDataStart = Off;
            MogpDataEnd = ChunkStart + ChunkSize;
            break;
        }

        Offset = ChunkStart + ChunkSize;
    }

    if (MogpDataStart == 0 || MogpDataEnd == 0)
    {
        UE_LOG(LogWmo, Warning, TEXT("WMO group: MOGP chunk not found"));
        return Result;
    }

    // Now iterate sub-chunks inside MOGP
    Offset = MogpDataStart;
    while (Offset + 8 <= MogpDataEnd)
    {
        const uint32 ChunkMagic = ReadU32(Base, Offset, DataSize);
        const uint32 ChunkSize = ReadU32(Base, Offset, DataSize);
        const uint32 ChunkStart = Offset;

        if (ChunkStart + ChunkSize > MogpDataEnd)
        {
            UE_LOG(LogWmo, Warning, TEXT("WMO group sub-chunk 0x%08X at %u overflows MOGP (size=%u)"),
                ChunkMagic, ChunkStart - 8, ChunkSize);
            break;
        }

        if (ChunkMagic == CHUNK_MOVI)
        {
            // Triangle indices: uint16 each
            const uint32 NumIndices = ChunkSize / 2;
            Result.Indices.SetNumUninitialized(NumIndices);
            uint32 Off = ChunkStart;
            for (uint32 i = 0; i < NumIndices; i++)
            {
                Result.Indices[i] = ReadU16(Base, Off, DataSize);
            }
        }
        else if (ChunkMagic == CHUNK_MOVT)
        {
            // Vertices: 3 floats (12 bytes) each
            const uint32 NumVerts = ChunkSize / 12;
            Result.Vertices.SetNumUninitialized(NumVerts);
            uint32 Off = ChunkStart;
            for (uint32 i = 0; i < NumVerts; i++)
            {
                const float X = ReadFloat(Base, Off, DataSize);
                const float Y = ReadFloat(Base, Off, DataSize);
                const float Z = ReadFloat(Base, Off, DataSize);
                Result.Vertices[i] = FVector(X, Y, Z);
            }
        }
        else if (ChunkMagic == CHUNK_MONR)
        {
            // Normals: 3 floats (12 bytes) each
            const uint32 NumNormals = ChunkSize / 12;
            Result.Normals.SetNumUninitialized(NumNormals);
            uint32 Off = ChunkStart;
            for (uint32 i = 0; i < NumNormals; i++)
            {
                const float X = ReadFloat(Base, Off, DataSize);
                const float Y = ReadFloat(Base, Off, DataSize);
                const float Z = ReadFloat(Base, Off, DataSize);
                Result.Normals[i] = FVector(X, Y, Z);
            }
        }
        else if (ChunkMagic == CHUNK_MOTV)
        {
            // Texture coordinates: 2 floats (8 bytes) each
            // Only store the first MOTV set (some groups have two)
            if (Result.TexCoords.Num() == 0)
            {
                const uint32 NumCoords = ChunkSize / 8;
                Result.TexCoords.SetNumUninitialized(NumCoords);
                uint32 Off = ChunkStart;
                for (uint32 i = 0; i < NumCoords; i++)
                {
                    const float U = ReadFloat(Base, Off, DataSize);
                    const float V = ReadFloat(Base, Off, DataSize);
                    Result.TexCoords[i] = FVector2D(U, V);
                }
            }
        }
        else if (ChunkMagic == CHUNK_MOBA)
        {
            // Render batches: 24 bytes each
            const uint32 NumBatches = ChunkSize / 24;
            Result.Batches.SetNumUninitialized(NumBatches);
            uint32 Off = ChunkStart;
            for (uint32 i = 0; i < NumBatches; i++)
            {
                FWmoGroupData::FBatch& Batch = Result.Batches[i];

                // Skip bounding box: 6 int16 = 12 bytes
                Off += 12;

                Batch.IndexStart    = ReadU32(Base, Off, DataSize);
                Batch.IndexCount    = ReadU16(Base, Off, DataSize);
                Batch.VertexStart   = ReadU16(Base, Off, DataSize);
                Batch.VertexEnd     = ReadU16(Base, Off, DataSize);
                /*flags*/             ReadU8(Base, Off, DataSize);
                Batch.MaterialIndex = ReadU8(Base, Off, DataSize);
            }
        }
        else if (ChunkMagic == CHUNK_MOCV)
        {
            // Vertex colors: BGRA, 4 bytes each
            // Only store the first MOCV set
            if (Result.VertexColors.Num() == 0)
            {
                const uint32 NumColors = ChunkSize / 4;
                Result.VertexColors.SetNumUninitialized(NumColors);
                uint32 Off = ChunkStart;
                for (uint32 i = 0; i < NumColors; i++)
                {
                    const uint8 B = ReadU8(Base, Off, DataSize);
                    const uint8 G = ReadU8(Base, Off, DataSize);
                    const uint8 R = ReadU8(Base, Off, DataSize);
                    const uint8 A = ReadU8(Base, Off, DataSize);
                    Result.VertexColors[i] = FColor(R, G, B, A);
                }
            }
        }

        // Advance to next sub-chunk
        Offset = ChunkStart + ChunkSize;
    }

    Result.bIsValid = Result.Vertices.Num() > 0 && Result.Indices.Num() > 0;
    UE_LOG(LogWmo, Log, TEXT("WMO group parsed: %d verts, %d indices, %d batches, %d colors"),
        Result.Vertices.Num(), Result.Indices.Num(), Result.Batches.Num(), Result.VertexColors.Num());
    return Result;
}
