#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
class WOWNETWORK_API FWowNetworkModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
