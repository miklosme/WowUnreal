#pragma once
#include "CoreMinimal.h"

/** Low-resolution heightmap for a single WDL tile */
struct WOWDATA_API FWdlTileData
{
	/** 17x17 outer vertices (int16 heights) */
	int16 Height17[17][17] = {};
	/** 16x16 inner midpoint vertices */
	int16 Height16[16][16] = {};
	/** MAHO hole bitmask — 16 uint16 values (one per row of the 16x16 inner grid).
	 *  Each uint16 has 16 bits, one per column. If bit is set, that quad is a hole. */
	uint16 HoleMask[16] = {};
	bool bHasData = false;
	bool bHasHoles = false;

	/** Check if the quad at (Col, Row) in the 16x16 grid is a hole */
	bool IsHole(int32 Col, int32 Row) const
	{
		if (!bHasHoles || Row < 0 || Row >= 16 || Col < 0 || Col >= 16) return false;
		return (HoleMask[Row] & (1 << Col)) != 0;
	}
};

/** Parsed WDL file — low-res heightmaps for all tiles in a map */
struct WOWDATA_API FWdlData
{
	TSharedPtr<FWdlTileData> Tiles[64][64];
	bool bIsValid = false;

	bool HasTile(int32 TX, int32 TY) const
	{
		if (TX < 0 || TX >= 64 || TY < 0 || TY >= 64) return false;
		return Tiles[TY][TX].IsValid() && Tiles[TY][TX]->bHasData;
	}
};
