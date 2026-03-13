#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WowViewerPlayerController.generated.h"

UCLASS()
class WOWUNREAL_API AWowViewerPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AWowViewerPlayerController();
protected:
    virtual void BeginPlay() override;
};
