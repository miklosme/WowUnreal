#include "WowGameplayController.h"
#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "WowEntity.h"
#include "WowOpcodes.h"
#include "WowPlayerCharacter.h"
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

void AWowGameplayController::OnLoginVerifyWorld(uint32 MapId, float X, float Y, float Z, float Orientation)
{
    FVector SpawnPos = FWowCoordinate::WowToUE(X, Y, Z);

    APawn* P = GetPawn();
    if (P)
    {
        if (AWowPlayerCharacter* PlayerCharacter = Cast<AWowPlayerCharacter>(P))
        {
            PlayerCharacter->ApplyLoginSpawn(SpawnPos, Orientation);
        }
        else
        {
            P->SetActorLocation(SpawnPos);
            P->SetActorRotation(FRotator(0.0f, FMath::RadiansToDegrees(Orientation), 0.0f));
        }

        UE_LOG(LogWowGameplay, Log, TEXT("Teleported to spawn: map=%d wow=(%.1f,%.1f,%.1f) orient=%.2f ue=(%.0f,%.0f,%.0f)"),
            MapId, X, Y, Z, Orientation, SpawnPos.X, SpawnPos.Y, SpawnPos.Z);
    }

    bHasServerPosition = true;
}

void AWowGameplayController::OnEntityUpdated(const FWowEntity& Entity)
{
    if (!ConnectionManager) return;

    // Only care about local player entity
    if (Entity.Guid != ConnectionManager->PacketHandler.EntityManager.LocalPlayerGuid) return;

    // Apply server speeds to character
    if (Entity.Movement.RunSpeed > 0.0f)
    {
        if (AWowPlayerCharacter* PlayerChar = Cast<AWowPlayerCharacter>(GetPawn()))
        {
            PlayerChar->ApplyServerSpeeds(Entity.Movement.RunSpeed, Entity.Movement.WalkSpeed);
        }
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

    FVector WowPos = FWowCoordinate::UEToWow(Pos);

    float Orientation = FMath::DegreesToRadians(P->GetActorRotation().Yaw);

    ConnectionManager->SendMovement(WowOpcode::MSG_MOVE_HEARTBEAT, WowPos, Orientation, 0);
}
