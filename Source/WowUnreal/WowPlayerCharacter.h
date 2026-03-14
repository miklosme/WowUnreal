#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WowPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class WOWUNREAL_API AWowPlayerCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AWowPlayerCharacter();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // ── Camera ────────────────────────────────────────────────────────────────
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MinCameraDistance = 200.0f;  // ~2 yards

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
    float MaxCameraDistance = 3000.0f; // ~30 yards

    // ── Input ─────────────────────────────────────────────────────────────────
    UPROPERTY()
    TObjectPtr<UInputMappingContext> GameplayContext;

    UPROPERTY()
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY()
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY()
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY()
    TObjectPtr<UInputAction> ZoomAction;

    // ── Movement ──────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float RunSpeed = 700.0f;  // 7.0 WoW units/s × 100 cm/unit

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float WalkSpeedFactor = 0.357f; // 2.5 / 7.0

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    bool bIsWalking = false;

    /** Apply speeds received from the server entity */
    void ApplyServerSpeeds(float ServerRunSpeed, float ServerWalkSpeed);

    /** Apply server spawn data and stabilize the camera until terrain collision exists locally. */
    void ApplyLoginSpawn(const FVector& SpawnPos, float OrientationRadians);

private:
    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void OnJumpPressed();
    void OnJumpReleased();
    void OnZoom(const FInputActionValue& Value);

    // Camera orbit state
    float CameraYaw = 0.0f;
    float CameraPitch = -20.0f; // slightly above
    bool bIsRightMouseDown = false;
};
