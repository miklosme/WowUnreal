#include "WowGameplayController.h"
#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "WowEntity.h"
#include "WowOpcodes.h"
#include "GameFramework/Character.h"
#include "Coord/WowCoordinate.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowGameplay, Log, All);

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

void AWowGameplayController::BindEntityEvents()
{
    if (!ConnectionManager) return;

    // Listen for login verify world — teleport pawn to spawn position
    ConnectionManager->PacketHandler.OnLoginVerifyWorld.AddUObject(
        this, &AWowGameplayController::OnLoginVerifyWorld);

    // Listen for entity updates — sync local player position from server
    ConnectionManager->PacketHandler.EntityManager.OnEntityUpdated.AddUObject(
        this, &AWowGameplayController::OnEntityUpdated);
}

void AWowGameplayController::OnLoginVerifyWorld(uint32 MapId, float X, float Y, float Z)
{
    FVector SpawnPos = FWowCoordinate::AdtToUE(X, Y, Z);

    APawn* P = GetPawn();
    if (P)
    {
        P->SetActorLocation(SpawnPos);
        UE_LOG(LogWowGameplay, Log, TEXT("Teleported to spawn: map=%d wow=(%.1f,%.1f,%.1f) ue=(%.0f,%.0f,%.0f)"),
            MapId, X, Y, Z, SpawnPos.X, SpawnPos.Y, SpawnPos.Z);
    }

    bHasServerPosition = true;
}

void AWowGameplayController::OnEntityUpdated(const FWowEntity& Entity)
{
    if (!ConnectionManager) return;

    // Only care about local player entity
    if (Entity.Guid != ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid) return;

    // Apply server speeds to character if available
    if (Entity.Movement.RunSpeed > 0.0f)
    {
        // Will apply speeds when character rendering is implemented
    }
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
    if (!P || !bHasServerPosition) return;

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
