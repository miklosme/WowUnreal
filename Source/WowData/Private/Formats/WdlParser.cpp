#include "Formats/WdlParser.h"
#include "Formats/ChunkId.h"
DEFINE_LOG_CATEGORY_STATIC(LogWdl, Log, All);

// On-disk reversed FourCCs
static constexpr uint32 WDL_MVER = MakeFourCC('R', 'E', 'V', 'M');
static constexpr uint32 WDL_MWMO = MakeFourCC('O', 'M', 'W', 'M');
static constexpr uint32 WDL_MWID = MakeFourCC('D', 'I', 'W', 'M');
static constexpr uint32 WDL_MODF = MakeFourCC('F', 'D', 'O', 'M');
static constexpr uint32 WDL_MAOF = MakeFourCC('F', 'O', 'A', 'M');
static constexpr uint32 WDL_MARE = MakeFourCC('E', 'R', 'A', 'M');
static constexpr uint32 WDL_MAHO = MakeFourCC('O', 'H', 'A', 'M');

FWdlData FWdlParser::Parse(const TArray<uint8>& Data)
{
	FWdlData Result;
	if (Data.Num() < 8) return Result;

	const uint8* Base = Data.GetData();
	const uint8* End = Base + Data.Num();
	const uint8* Ptr = Base;

	// First pass: find MAOF offset table
	uint32 MareOffsets[64][64] = {};
	bool bFoundMAOF = false;

	while (Ptr + 8 <= End)
	{
		uint32 Magic = *reinterpret_cast<const uint32*>(Ptr);
		uint32 Size = *reinterpret_cast<const uint32*>(Ptr + 4);
		const uint8* ChunkData = Ptr + 8;

		if (ChunkData + Size > End) break;

		if (Magic == WDL_MVER)
		{
			if (Size >= 4)
			{
				uint32 Version = *reinterpret_cast<const uint32*>(ChunkData);
				if (Version != 18)
				{
					UE_LOG(LogWdl, Warning, TEXT("WDL unexpected version: %u"), Version);
					return Result;
				}
			}
		}
		else if (Magic == WDL_MAOF)
		{
			if (Size >= 64 * 64 * sizeof(uint32))
			{
				FMemory::Memcpy(MareOffsets, ChunkData, 64 * 64 * sizeof(uint32));
				bFoundMAOF = true;
			}
		}

		Ptr = ChunkData + Size;
	}

	if (!bFoundMAOF)
	{
		UE_LOG(LogWdl, Warning, TEXT("WDL: no MAOF chunk found"));
		return Result;
	}

	// Second pass: read MARE chunks by offset
	int32 TileCount = 0;
	int32 HoleCount = 0;
	for (int32 y = 0; y < 64; y++)
	{
		for (int32 x = 0; x < 64; x++)
		{
			uint32 Offset = MareOffsets[y][x];
			if (Offset == 0) continue;

			// Validate offset + header fits in file
			if (Offset + 8 > (uint32)Data.Num()) continue;

			const uint8* TilePtr = Base + Offset;
			uint32 TileMagic = *reinterpret_cast<const uint32*>(TilePtr);
			uint32 TileSize = *reinterpret_cast<const uint32*>(TilePtr + 4);

			if (TileMagic != WDL_MARE) continue;
			if (TileSize != 0x442) continue; // 17*17*2 + 16*16*2 = 578 + 512 = 1090 = 0x442

			const uint8* HeightData = TilePtr + 8;
			if (HeightData + TileSize > End) continue;

			TSharedPtr<FWdlTileData> Tile = MakeShared<FWdlTileData>();

			// Read 17x17 outer heights (int16)
			FMemory::Memcpy(Tile->Height17, HeightData, 17 * 17 * sizeof(int16));
			// Read 16x16 inner heights (int16)
			FMemory::Memcpy(Tile->Height16, HeightData + 17 * 17 * sizeof(int16), 16 * 16 * sizeof(int16));

			// Check for MAHO chunk following MARE
			const uint8* MahoPtr = HeightData + TileSize; // right after MARE data
			if (MahoPtr + 8 <= End)
			{
				uint32 MahoMagic = *reinterpret_cast<const uint32*>(MahoPtr);
				uint32 MahoSize = *reinterpret_cast<const uint32*>(MahoPtr + 4);
				if (MahoMagic == WDL_MAHO && MahoSize == 32 && MahoPtr + 8 + MahoSize <= End)
				{
					FMemory::Memcpy(Tile->HoleMask, MahoPtr + 8, 32);
					Tile->bHasHoles = true;
					HoleCount++;
				}
			}

			Tile->bHasData = true;
			Result.Tiles[y][x] = Tile;
			TileCount++;
		}
	}

	Result.bIsValid = TileCount > 0;
	UE_LOG(LogWdl, Log, TEXT("WDL parsed: %d tiles with height data (%d with holes)"), TileCount, HoleCount);
	return Result;
}
