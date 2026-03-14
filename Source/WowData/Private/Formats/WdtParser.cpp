#include "Formats/WdtParser.h"
DEFINE_LOG_CATEGORY_STATIC(LogWdt, Log, All);

namespace
{
// WoW stores chunk IDs as reversed FourCC on disk.
// "MPHD" is stored as bytes D,H,P,M. Read as LE uint32: M | P<<8 | H<<16 | D<<24
constexpr uint32 MakeFourCC(char A, char B, char C, char D)
{
    return (uint32)A | ((uint32)B << 8) | ((uint32)C << 16) | ((uint32)D << 24);
}

// These match what's ON DISK (reversed)
constexpr uint32 CHUNK_MVER = MakeFourCC('R', 'E', 'V', 'M');
constexpr uint32 CHUNK_MPHD = MakeFourCC('D', 'H', 'P', 'M');
constexpr uint32 CHUNK_MAIN = MakeFourCC('N', 'I', 'A', 'M');
constexpr uint32 CHUNK_MWMO = MakeFourCC('O', 'M', 'W', 'M');
}

FWdtData FWdtParser::Parse(const TArray<uint8>& Data)
{
    FWdtData Result;
    if (Data.Num() < 8) return Result;

    const uint8* Ptr = Data.GetData();
    const uint8* End = Ptr + Data.Num();

    while (Ptr + 8 <= End)
    {
        uint32 Magic = *reinterpret_cast<const uint32*>(Ptr);
        uint32 Size  = *reinterpret_cast<const uint32*>(Ptr + 4);
        const uint8* ChunkData = Ptr + 8;

        if (ChunkData + Size > End) break;

        if (Magic == CHUNK_MPHD)
        {
            if (Size >= 4)
            {
                Result.Flags = *reinterpret_cast<const uint32*>(ChunkData);
                Result.bUseBigAlpha = (Result.Flags & 0x04) != 0;
            }
        }
        else if (Magic == CHUNK_MAIN)
        {
            if (Size >= 64 * 64 * 8)
            {
                int32 TileCount = 0;
                for (int32 y = 0; y < 64; y++)
                {
                    for (int32 x = 0; x < 64; x++)
                    {
                        uint32 Flags = *reinterpret_cast<const uint32*>(ChunkData + (y * 64 + x) * 8);
                        Result.TileExists[x][y] = (Flags & 0x01) != 0;
                        if (Flags & 0x01) TileCount++;
                    }
                }
                UE_LOG(LogWdt, Log, TEXT("MAIN chunk: found %d existing tiles"), TileCount);
            }
        }
        else if (Magic == CHUNK_MWMO)
        {
            if (Size > 0)
            {
                Result.GlobalWmoPath = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ChunkData)));
            }
        }

        Ptr = ChunkData + Size;
    }

    Result.bIsValid = true;
    UE_LOG(LogWdt, Log, TEXT("WDT parsed: flags=0x%X, bigAlpha=%d, globalWmo='%s'"),
        Result.Flags, Result.bUseBigAlpha ? 1 : 0, *Result.GlobalWmoPath);
    return Result;
}
