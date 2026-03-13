#include "WowWorldManager.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowWorld, Log, All);

AWowWorldManager::AWowWorldManager() { PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickInterval = 0.5f; }

void AWowWorldManager::BeginPlay()
{
    Super::BeginPlay();
    MpqManager = MakeUnique<FMpqManager>();
    AssetCache = MakeUnique<FWowAssetCache>();
    FString DataPath;
    FParse::Value(FCommandLine::Get(), TEXT("-wowdata="), DataPath);
    if (DataPath.IsEmpty()) DataPath = TEXT("/Users/clancey/Downloads/World of Warcraft 3.3.5a/Data");
    if (MpqManager->Initialize(DataPath))
        UE_LOG(LogWowWorld, Log, TEXT("World Manager ready: %s"), *MapName);
    else
        UE_LOG(LogWowWorld, Error, TEXT("Failed to init MPQ from: %s"), *DataPath);
}

void AWowWorldManager::EndPlay(const EEndPlayReason::Type R)
{
    if (AssetCache) AssetCache->Clear();
    if (MpqManager) MpqManager->Shutdown();
    Super::EndPlay(R);
}

void AWowWorldManager::Tick(float DT) { Super::Tick(DT); }
