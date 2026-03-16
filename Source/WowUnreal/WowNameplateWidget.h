#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "WowNameplateWidget.generated.h"

UCLASS()
class WOWUNREAL_API UWowNameplateWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Initialize the nameplate with entity data */
    void SetupNameplate(const FString& EntityName, int32 Health, int32 MaxHealth, bool bIsHostile = false);

    /** Update health values */
    void UpdateHealth(int32 Health, int32 MaxHealth);

protected:
    virtual void NativeConstruct() override;

    /** Text block for entity name (optional bind widget) */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> NameText;

    /** Progress bar for health (optional bind widget) */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UProgressBar> HealthBar;

private:
    bool bIsHostileEntity = false;
};