#include "Formats/WmoParser.h"
DEFINE_LOG_CATEGORY_STATIC(LogWmo, Log, All);
FWmoRootData FWmoParser::ParseRoot(const TArray<uint8>& Data) { FWmoRootData R; UE_LOG(LogWmo, Warning, TEXT("WMO root stub")); return R; }
FWmoGroupData FWmoParser::ParseGroup(const TArray<uint8>& Data) { FWmoGroupData R; UE_LOG(LogWmo, Warning, TEXT("WMO group stub")); return R; }
