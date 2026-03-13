#include "WowData.h"
void FWowDataModule::StartupModule() { UE_LOG(LogTemp, Log, TEXT("WowData module started")); }
void FWowDataModule::ShutdownModule() {}
IMPLEMENT_MODULE(FWowDataModule, WowData);
