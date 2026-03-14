#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "WowDebugHUD.generated.h"

UCLASS()
class WOWUNREAL_API AWowDebugHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;

private:
    void DrawTextLine(float& Y, const FString& Text, FLinearColor Color = FLinearColor::White);
    float LineHeight = 18.0f;
    float Margin = 10.0f;
};
