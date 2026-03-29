#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WowMapCreatorCommandlet.generated.h"

/**
 * Creates empty .umap files for WowUnreal test/production maps.
 * Usage: UnrealEditor WowUnreal.uproject -run=WowMapCreator
 */
UCLASS()
class UWowMapCreatorCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    virtual int32 Main(const FString& Params) override;

private:
    bool CreateMap(const FString& MapName, const FString& GameModeClassPath);
};
