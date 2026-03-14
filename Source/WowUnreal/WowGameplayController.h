#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WowGameplayController.generated.h"

class UWowConnectionManager;
struct FWowEntity;

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

    /** Wire entity events from the packet handler (call after setting ConnectionManager) */
    void BindEntityEvents();

private:
    // Movement sync
    void SendMovementUpdate();
    FVector LastSentPosition = FVector::ZeroVector;
    float MovementSyncTimer = 0.0f;
    float MovementSyncInterval = 0.5f; // 500ms heartbeat

    // Keep-alive
    float KeepAliveTimer = 0.0f;
    float KeepAliveInterval = 30.0f;

    // Server position sync
    void OnLoginVerifyWorld(uint32 MapId, float X, float Y, float Z);
    void OnEntityUpdated(const FWowEntity& Entity);
    bool bHasServerPosition = false;
};
