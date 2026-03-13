#pragma once
#include "CoreMinimal.h"

struct WOWDATA_API FWdtData
{
    bool TileExists[64][64] = {};
    uint32 Flags = 0;
    bool bUseBigAlpha = false;
    FString GlobalWmoPath;
    bool bIsValid = false;
};
