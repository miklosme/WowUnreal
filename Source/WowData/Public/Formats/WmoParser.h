#pragma once
#include "CoreMinimal.h"
#include "Formats/WmoTypes.h"

class WOWDATA_API FWmoParser
{
public:
    static FWmoRootData ParseRoot(const TArray<uint8>& Data);
    static FWmoGroupData ParseGroup(const TArray<uint8>& Data);
};
