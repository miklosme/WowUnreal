#pragma once
#include "CoreMinimal.h"
#include "Formats/WdlTypes.h"

class WOWDATA_API FWdlParser
{
public:
	static FWdlData Parse(const TArray<uint8>& Data);
};
