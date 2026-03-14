#pragma once
#include "CoreMinimal.h"

/** Low-resolution heightmap for a single WDL tile */
struct WOWDATA_API FWdlTileData
{
	/** 17x17 outer vertices (int16 heights) */
	int16 Height17[17][17] = {};
	/** 16x16 inner midpoint vertices */
	int16 Height16[16][16] = {};
	bool bHasData = false;
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
