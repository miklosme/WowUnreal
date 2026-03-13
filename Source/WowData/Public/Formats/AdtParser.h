#pragma once
#include "CoreMinimal.h"
#include "Formats/AdtTypes.h"

class WOWDATA_API FAdtParser
{
public:
    static FAdtData Parse(const TArray<uint8>& Data, bool bBigAlpha = false);
};
