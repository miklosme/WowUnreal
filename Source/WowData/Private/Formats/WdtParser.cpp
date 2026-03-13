#include "Formats/WdtParser.h"
DEFINE_LOG_CATEGORY_STATIC(LogWdt, Log, All);
FWdtData FWdtParser::Parse(const TArray<uint8>& Data)
{
    FWdtData R;
    UE_LOG(LogWdt, Warning, TEXT("WDT parser stub"));
    return R;
}
