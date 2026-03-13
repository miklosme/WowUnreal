#pragma once
#include "CoreMinimal.h"
#include "WowScreenshotManager.generated.h"

UCLASS(BlueprintType)
class WOWCLIENT_API UWowScreenshotManager : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) FString TakeScreenshot();
    UFUNCTION(BlueprintCallable) FString GetScreenshotDir() const;
};
