#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "WowFloatingText.generated.h"

USTRUCT(BlueprintType)
struct WOWUNREAL_API FWowFloatingTextInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString Text;

    UPROPERTY(BlueprintReadWrite)
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(BlueprintReadWrite)
    float Duration = 2.0f;

    UPROPERTY(BlueprintReadWrite)
    float Speed = 100.0f; // Units per second to float upward

    UPROPERTY(BlueprintReadWrite)
    float FontSize = 24.0f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WOWUNREAL_API UWowFloatingText : public USceneComponent
{
    GENERATED_BODY()

public:
    UWowFloatingText();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /** Spawn floating text above an actor */
    UFUNCTION(BlueprintCallable)
    static UWowFloatingText* SpawnFloatingText(AActor* TargetActor, const FWowFloatingTextInfo& TextInfo);

    /** Start the floating text animation */
    UFUNCTION(BlueprintCallable)
    void StartFloating(const FWowFloatingTextInfo& TextInfo);

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UTextRenderComponent> TextRenderComponent;

    bool bIsFloating = false;
    float FloatTimer = 0.0f;
    float FloatDuration = 2.0f;
    float FloatSpeed = 100.0f;
    FVector StartLocation;
    FLinearColor BaseColor = FLinearColor::White;

    void UpdateFloating(float DeltaTime);
};