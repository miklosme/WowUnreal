#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WowWorldManager.generated.h"

class FMpqManager;
class FWowAssetCache;

UCLASS()
class WOWWORLD_API AWowWorldManager : public AActor
{
    GENERATED_BODY()
public:
    AWowWorldManager();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;

    FMpqManager* GetMpqManager() const { return MpqManager.Get(); }
    FWowAssetCache* GetAssetCache() const { return AssetCache.Get(); }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW")
    FString MapName = TEXT("Azeroth");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 LoadRadius = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Streaming")
    int32 UnloadRadius = 4;
private:
    TUniquePtr<FMpqManager> MpqManager;
    TUniquePtr<FWowAssetCache> AssetCache;
};
