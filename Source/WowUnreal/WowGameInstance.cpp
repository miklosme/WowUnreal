#include "WowGameInstance.h"

UWowGameInstance::UWowGameInstance() : WowDataPath(TEXT("")) {}

void UWowGameInstance::Init()
{
    Super::Init();
    if (WowDataPath.IsEmpty())
    {
        FParse::Value(FCommandLine::Get(), TEXT("-wowdata="), WowDataPath);
    }
    UE_LOG(LogTemp, Log, TEXT("WoW Data Path: %s"), *WowDataPath);
}
