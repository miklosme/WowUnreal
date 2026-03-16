#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Components/WidgetComponent.h"
#include "WowGameplayController.generated.h"

class UWowConnectionManager;
class UWowUIManager;
class AWowWorldManager;
class UWowDeathManager;
class UWowCursorManager;
class UWowTooltipManager;
class FMpqManager;
class FWowAssetCache;
class SWowCombatLog;
struct FWowEntity;

UCLASS()
class WOWUNREAL_API AWowGameplayController : public APlayerController
{
    GENERATED_BODY()
public:
    AWowGameplayController();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupInputComponent() override;

    /** Connection manager for sending movement packets */
    UPROPERTY()
    TObjectPtr<UWowConnectionManager> ConnectionManager;

    /** UI manager for event dispatch */
    UPROPERTY()
    TObjectPtr<UWowUIManager> UIManager;

    /** Death manager for player death handling */
    UPROPERTY()
    TObjectPtr<UWowDeathManager> DeathManager;

    /** Cursor manager for WoW custom cursors */
    UPROPERTY()
    TObjectPtr<UWowCursorManager> CursorManager;

    /** Tooltip manager for entity tooltips */
    UPROPERTY()
    TObjectPtr<UWowTooltipManager> TooltipManager;

    /** Wire entity events from the packet handler (call after setting ConnectionManager) */
    void BindEntityEvents();

    /** Initialize all managers */
    void InitializeManagers();

    /** Currently targeted entity GUID */
    UPROPERTY()
    uint64 TargetGuid = 0;

    /** Add a message to the combat log */
    void AddCombatMessage(const FString& Message, const FLinearColor& Color);

    /** Combat log widget for displaying spell casts, damage, etc. */
    TSharedPtr<SWowCombatLog> CombatLog;

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
    void OnLoginVerifyWorld(uint32 MapId, float X, float Y, float Z, float Orientation);
    void OnEntityUpdated(const FWowEntity& Entity);

    // Teleport handling
    void OnTeleportRequest(uint64 Guid, uint32 Flags, uint32 Time, FVector Position, float Orientation);
    void OnMapTransfer(uint32 MapId, float X, float Y, float Z, float Orientation);
    void SendTeleportAck(uint64 Guid, uint32 Flags, uint32 Time);
    void SendWorldportAck();
    bool bHasServerPosition = false;

public:
    /** Deferred spawn: store the position but don't teleport yet */
    bool bDeferSpawnTeleport = false;
    FVector DeferredSpawnPos = FVector::ZeroVector;
    float DeferredSpawnOrientation = 0.0f;
    bool bHasDeferredSpawn = false;

    /** Apply the deferred spawn position now (called by LoginController after terrain loads) */
    void ApplyDeferredSpawn();

private:
    void ApplyDeferredSpawn_Internal(const FVector& SpawnPos, float Orientation);

    // Server position correction
    FVector LastServerPosition = FVector::ZeroVector;
    float ServerCorrectionThreshold = 500.0f; // 5 WoW yards — teleport if diverged

    // Opcode → UI event forwarding
    void OnOpcodeReceived(uint16 Opcode);

    // Targeting
    void OnLeftClick();
    void TryTargetUnderCursor();

    // Entity model spawning
    void OnEntityCreated(const FWowEntity& Entity);
    void OnEntityDestroyed(uint64 Guid);
    void SpawnEntityModel(const FWowEntity& Entity);
    void SetupLocalPlayerCharacterModel(const FWowEntity& Entity);
    void CacheWorldResources();

    // Entity movement interpolation
    void UpdateEntitySplineMovement(float DeltaTime);

    UPROPERTY()
    TMap<uint64, TObjectPtr<AActor>> SpawnedEntityActors;

    UPROPERTY()
    TMap<uint64, TObjectPtr<UWidgetComponent>> EntityNameplates;

    FMpqManager* CachedMpq = nullptr;
    FWowAssetCache* CachedAssetCache = nullptr;

    // Combat log event handlers
    void OnSpellStart(uint64 CasterGuid, uint32 SpellId, uint32 CastFlags, int32 CastTime);
    void OnChatMessage(const FString& Message);
    void OnEntityHealthChanged(const FWowEntity& Entity, int32 OldHealth, int32 NewHealth);
};
