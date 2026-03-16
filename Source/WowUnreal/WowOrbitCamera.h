#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "WowOrbitCamera.generated.h"

/**
 * Orbit camera for Model Viewer. Rotates around a focal point.
 * LMB drag = rotate, Scroll = zoom, MMB drag = pan.
 */
UCLASS()
class WOWUNREAL_API AWowOrbitCamera : public ASpectatorPawn
{
    GENERATED_BODY()
public:
    AWowOrbitCamera();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    /** Set the point the camera orbits around */
    void SetFocalPoint(const FVector& InFocalPoint);

    UPROPERTY(EditAnywhere, Category = "Orbit")
    FVector FocalPoint = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category = "Orbit")
    float OrbitYaw = 180.0f;

    UPROPERTY(EditAnywhere, Category = "Orbit")
    float OrbitPitch = 10.0f;

    UPROPERTY(EditAnywhere, Category = "Orbit")
    float OrbitDistance = 250.0f;

    UPROPERTY(EditAnywhere, Category = "Orbit")
    float MinDistance = 50.0f;

    UPROPERTY(EditAnywhere, Category = "Orbit")
    float MaxDistance = 5000.0f;

    UPROPERTY(EditAnywhere, Category = "Orbit")
    float RotateSpeed = 0.3f;

    UPROPERTY(EditAnywhere, Category = "Orbit")
    float PanSpeed = 1.0f;

    UPROPERTY(EditAnywhere, Category = "Orbit")
    float ZoomSpeed = 30.0f;

private:
    bool bLeftMouseDown = false;
    bool bMiddleMouseDown = false;
    FVector2D LastMousePos = FVector2D::ZeroVector;

    void UpdateCameraTransform();
};
