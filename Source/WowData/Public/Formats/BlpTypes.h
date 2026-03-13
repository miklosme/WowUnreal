#pragma once
#include "CoreMinimal.h"

enum class EBlpPixelFormat : uint8 { DXT1, DXT3, DXT5, RGBA8, Unknown };

struct FBlpMipLevel
{
    TArray<uint8> Data;
    uint32 Width = 0;
    uint32 Height = 0;
};

struct WOWDATA_API FBlpTexture
{
    uint32 Width = 0;
    uint32 Height = 0;
    EBlpPixelFormat PixelFormat = EBlpPixelFormat::Unknown;
    TArray<FBlpMipLevel> MipLevels;
    bool bIsValid = false;
};
