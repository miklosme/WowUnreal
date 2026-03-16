#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WowPlayerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UWowAnimationController;
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
    virtual void Landed(const FHitResult& Hit) override;

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

    UPROPERTY()
    TObjectPtr<UInputAction> ToggleWalkAction;

    UPROPERTY()
    TObjectPtr<UInputAction> AutoRunAction;

    // ── Movement ──────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float RunSpeed = 700.0f;  // 7.0 WoW units/s × 100 cm/unit

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float WalkSpeedFactor = 0.357f; // 2.5 / 7.0

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float BackpedalFactor = 0.6f; // 60% of forward speed

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    bool bIsWalking = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    bool bIsAutoRunning = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float SwimSpeedFactor = 0.67f; // 67% of run speed

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float FallDamageMinHeight = 1500.0f; // ~15 WoW yards

    /** Apply speeds received from the server entity */
    void ApplyServerSpeeds(float ServerRunSpeed, float ServerWalkSpeed);

    /** Apply server spawn data */
    void ApplyLoginSpawn(const FVector& SpawnPos, float OrientationRadians);

    /** Set the character's visual model based on race/gender/customization */
    void SetCharacterModel(UWorld* World, class FMpqManager* Mpq, class FWowAssetCache* Cache,
        uint8 Race, uint8 Gender, uint8 SkinColor = 0, uint8 Face = 0, uint8 HairStyle = 0,
        uint8 HairColor = 0, uint8 FacialHair = 0);

    /** Setup animation controller for local player character model */
    void SetupAnimationController(class UWowAnimationController* AnimController);

private:
    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void OnJumpPressed();
    void OnJumpReleased();
    void OnZoom(const FInputActionValue& Value);
    void OnToggleWalk();
    void OnToggleAutoRun();

    // Camera orbit state
    float CameraYaw = 0.0f;
    float CameraPitch = -20.0f;

    // Fall tracking
    float FallStartZ = 0.0f;
    bool bIsFalling = false;

    // Left mouse drag turns character
    bool bLeftMouseTurning = false;

    // Animation controller for character model (if using WoW character model)
    UPROPERTY()
    TObjectPtr<UWowAnimationController> AnimationController;
};
