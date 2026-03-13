#pragma once
#include "CoreMinimal.h"
#include "Formats/M2Types.h"

class WOWDATA_API FM2Parser
{
public:
    static FM2Data Parse(const TArray<uint8>& M2Data, const TArray<uint8>& SkinData);
};
