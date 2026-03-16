#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "WowGameplayController.generated.h"

class UWowConnectionManager;
class UWowUIManager;
class AWowWorldManager;
class FMpqManager;
class FWowAssetCache;
struct FWowEntity;
class SWowActionBar;

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

    /** Wire entity events from the packet handler (call after setting ConnectionManager) */
    void BindEntityEvents();

    /** Currently targeted entity GUID */
    UPROPERTY()
    uint64 TargetGuid = 0;

    /** Action bar widget */
    TSharedPtr<SWowActionBar> ActionBarWidget;

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
    bool bHasServerPosition = false;

public:
    /** Deferred spawn: store the position but don't teleport yet */
    bool bDeferSpawnTeleport = false;
    FVector DeferredSpawnPos = FVector::ZeroVector;
    float DeferredSpawnOrientation = 0.0f;
    bool bHasDeferredSpawn = false;

    /** Apply the deferred spawn position now (called by LoginController after terrain loads) */
    void ApplyDeferredSpawn();

    /** Create and show the action bar widget */
    void CreateActionBarWidget();

    /** Handle keyboard input for action bar slots */
    void HandleActionBarInput();

private:
    // Action bar input handling
    void OnActionSlot1() { CastSpellFromSlot(0); }
    void OnActionSlot2() { CastSpellFromSlot(1); }
    void OnActionSlot3() { CastSpellFromSlot(2); }
    void OnActionSlot4() { CastSpellFromSlot(3); }
    void OnActionSlot5() { CastSpellFromSlot(4); }
    void OnActionSlot6() { CastSpellFromSlot(5); }
    void OnActionSlot7() { CastSpellFromSlot(6); }
    void OnActionSlot8() { CastSpellFromSlot(7); }
    void OnActionSlot9() { CastSpellFromSlot(8); }
    void OnActionSlot0() { CastSpellFromSlot(9); }
    void OnActionSlotMinus() { CastSpellFromSlot(10); }
    void OnActionSlotEquals() { CastSpellFromSlot(11); }

    void CastSpellFromSlot(int32 SlotIndex);

    // Name query handling
    void OnPlayerNameReceived(uint64 Guid, const FString& Name);
    void OnCreatureNameReceived(uint32 Entry, const FString& Name, const FString& Title);

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
    void CacheWorldResources();

    UPROPERTY()
    TMap<uint64, TObjectPtr<AActor>> SpawnedEntityActors;

    FMpqManager* CachedMpq = nullptr;
    FWowAssetCache* CachedAssetCache = nullptr;
};
