#pragma once
#include "CoreMinimal.h"
#include "Formats/WdtTypes.h"

class WOWDATA_API FWdtParser
{
public:
    static FWdtData Parse(const TArray<uint8>& Data);
};
