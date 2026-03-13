#include "Formats/WdtParser.h"
DEFINE_LOG_CATEGORY_STATIC(LogWdt, Log, All);

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

        // MPHD — map header flags (32 bytes: uint32 flags + 7 uint32 padding)
        if (Magic == 'DHPM') // "MPHD" reversed
        {
            if (Size >= 4)
            {
                Result.Flags = *reinterpret_cast<const uint32*>(ChunkData);
                Result.bUseBigAlpha = (Result.Flags & 0x04) != 0;
            }
        }
        // MAIN — 64x64 tile entries, each 8 bytes (uint32 flags, uint32 asyncId)
        else if (Magic == 'NIAM') // "MAIN" reversed
        {
            if (Size >= 64 * 64 * 8)
            {
                for (int32 y = 0; y < 64; y++)
                {
                    for (int32 x = 0; x < 64; x++)
                    {
                        uint32 Flags = *reinterpret_cast<const uint32*>(ChunkData + (y * 64 + x) * 8);
                        Result.TileExists[x][y] = (Flags & 0x01) != 0;
                    }
                }
            }
        }
        // MWMO — global WMO path (for dungeon/raid maps that are a single WMO)
        else if (Magic == 'OMWM') // "MWMO" reversed
        {
            if (Size > 0)
            {
                Result.GlobalWmoPath = FString(UTF8_TO_TCHAR(reinterpret_cast<const char*>(ChunkData)));
            }
        }
        // MODF and MVER are present but we don't need to extract data from them right now

        Ptr = ChunkData + Size;
    }

    Result.bIsValid = true;
    UE_LOG(LogWdt, Log, TEXT("WDT parsed: flags=0x%X, bigAlpha=%d, globalWmo='%s'"),
        Result.Flags, Result.bUseBigAlpha ? 1 : 0, *Result.GlobalWmoPath);
    return Result;
}
