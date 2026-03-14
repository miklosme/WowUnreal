#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WowGameplayController.generated.h"

class UWowConnectionManager;

UCLASS()
class WOWUNREAL_API AWowGameplayController : public APlayerController
{
    GENERATED_BODY()
public:
    AWowGameplayController();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    /** Connection manager for sending movement packets */
    UPROPERTY()
    TObjectPtr<UWowConnectionManager> ConnectionManager;

private:
    // Movement sync
    void SendMovementUpdate();
    FVector LastSentPosition = FVector::ZeroVector;
    float MovementSyncTimer = 0.0f;
    float MovementSyncInterval = 0.5f; // 500ms heartbeat

    // Keep-alive
    float KeepAliveTimer = 0.0f;
    float KeepAliveInterval = 30.0f;
};
