#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "WowGameInstance.generated.h"

UCLASS()
class WOWUNREAL_API UWowGameInstance : public UGameInstance
{
    GENERATED_BODY()
public:
    UWowGameInstance();
    virtual void Init() override;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "WoW")
    FString WowDataPath;
};
