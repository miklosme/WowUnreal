#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "WowFlyCamera.generated.h"

class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class WOWUNREAL_API AWowFlyCamera : public ASpectatorPawn
{
    GENERATED_BODY()
public:
    AWowFlyCamera();
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputMappingContext> FlyCameraContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    TObjectPtr<UInputAction> SpeedAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float FlySpeed = 20000.0f;  // 200 m/s default, use mouse wheel to adjust

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float SprintMultiplier = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MinSpeed = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MaxSpeed = 50000.0f;

private:
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void AdjustSpeed(const FInputActionValue& Value);
    bool bIsSprinting = false;
};
