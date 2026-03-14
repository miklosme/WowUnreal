#include "WowGameplayController.h"
#include "WowConnectionManager.h"
#include "WowOpcodes.h"
#include "GameFramework/Character.h"
#include "Coord/WowCoordinate.h"

AWowGameplayController::AWowGameplayController()
{
    bShowMouseCursor = false;
    PrimaryActorTick.bCanEverTick = true;
}

void AWowGameplayController::BeginPlay()
{
    Super::BeginPlay();
    SetInputMode(FInputModeGameOnly());
}

void AWowGameplayController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!ConnectionManager) return;

    // Movement heartbeat — send position to server while moving
    MovementSyncTimer += DeltaTime;
    if (MovementSyncTimer >= MovementSyncInterval)
    {
        MovementSyncTimer = 0.0f;
        SendMovementUpdate();
    }

    // Keep-alive
    KeepAliveTimer += DeltaTime;
    if (KeepAliveTimer >= KeepAliveInterval)
    {
        KeepAliveTimer = 0.0f;
        ConnectionManager->SendKeepAlive();
    }
}

void AWowGameplayController::SendMovementUpdate()
{
    APawn* P = GetPawn();
    if (!P) return;

    FVector Pos = P->GetActorLocation();

    // Only send if position actually changed
    if (Pos.Equals(LastSentPosition, 1.0f)) return;
    LastSentPosition = Pos;

    // Convert UE position back to WoW coordinates for the server
    // UE: X=-NgZ*S, Y=NgX*S, Z=NgY*S  →  WoW: NgX=UE.Y/S, NgY=UE.Z/S, NgZ=-UE.X/S
    static constexpr float INV_SCALE = 1.0f / FWowCoordinate::SCALE;
    FVector WowPos(Pos.Y * INV_SCALE, Pos.Z * INV_SCALE, -Pos.X * INV_SCALE);

    float Orientation = FMath::DegreesToRadians(P->GetActorRotation().Yaw);

    ConnectionManager->SendMovement(WowOpcode::MSG_MOVE_HEARTBEAT, WowPos, Orientation, 0);
}
