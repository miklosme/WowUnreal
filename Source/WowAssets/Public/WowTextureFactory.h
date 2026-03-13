#pragma once
#include "CoreMinimal.h"
#include "Formats/BlpTypes.h"
class UTexture2D;
class WOWASSETS_API FWowTextureFactory
{
public:
    static UTexture2D* CreateTexture(const FBlpTexture& BlpData, const FString& Name = TEXT("WowTex"));
};
