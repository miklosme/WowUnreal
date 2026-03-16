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
class SWowCastBar;
struct FWowFloatingTextInfo;

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

    /** Cast a spell on the current target (or self if no target) */
    UFUNCTION(BlueprintCallable)
    void CastSpell(int32 SpellId);

    /** Start auto-attack on targeted entity (right-click) */
    void StartAutoAttack();

    /** Stop auto-attack */
    void StopAutoAttack();

    /** Get auto-attack state */
    UFUNCTION(BlueprintCallable)
    bool IsAutoAttacking() const { return bIsAutoAttacking; }

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

    // Cast bar widget
    TSharedPtr<SWowCastBar> CastBarWidget;

    // Combat state
    bool bIsAutoAttacking = false;
    uint64 AutoAttackTargetGuid = 0;

    // Spell casting
    bool bIsCasting = false;
    int32 CurrentSpellId = 0;
    float CastStartTime = 0.0f;
    float CastDuration = 0.0f;

    // Combat event handlers
    void OnSpellStart(uint64 CasterGuid, uint32 SpellId, uint32 CastFlags, int32 CastTime);
    void OnSpellGo(uint64 CasterGuid, uint32 SpellId, uint32 CastFlags);
    void OnSpellFailure(uint64 CasterGuid, uint32 SpellId, uint8 FailureReason);
    void OnAttackerStateUpdate(uint64 AttackerGuid, uint64 VictimGuid, uint32 HitInfo, uint32 Damage);

    // Input handlers for spell casting (1-6 keys)
    void OnSpellKey1() { CastSpell(133); } // Fireball
    void OnSpellKey2() { CastSpell(116); } // Frostbolt
    void OnSpellKey3() { CastSpell(2136); } // Fire Blast
    void OnSpellKey4() { CastSpell(122); }  // Frost Nova
    void OnSpellKey5() { CastSpell(1449); } // Arcane Explosion
    void OnSpellKey6() { CastSpell(475); }  // Remove Curse

    void OnRightClick();
};
