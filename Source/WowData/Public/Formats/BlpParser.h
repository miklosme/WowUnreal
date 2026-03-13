#pragma once
#include "CoreMinimal.h"
#include "Formats/BlpTypes.h"

class WOWDATA_API FBlpParser
{
public:
    static FBlpTexture Parse(const TArray<uint8>& Data);
private:
    static FBlpTexture ParseDXT(const uint8* Data, uint32 DataSize, uint32 Width, uint32 Height, uint8 AlphaDepth, uint8 AlphaEncoding, const uint32* MipOffsets, const uint32* MipSizes);
    static FBlpTexture ParsePaletted(const uint8* Data, uint32 DataSize, uint32 Width, uint32 Height, uint8 AlphaDepth, const uint32* MipOffsets, const uint32* MipSizes);
};
