#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
class WOWASSETS_API FWowAssetsModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
