#pragma once
#include "CoreMinimal.h"
#include "WowAnimationController.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;
struct FM2AnimationData;

/**
 * WoW M2 Animation IDs from AnimationData.dbc
 * Maps to specific character actions for state machine control.
 */
UENUM(BlueprintType)
enum class EWowAnimId : uint8
{
    Stand = 0,          // Idle standing
    Death = 1,          // Death animation
    Walk = 4,           // Walking
    Run = 5,            // Running
    Wound = 13,         // Hit reaction
    SpellCast = 15,     // Directed spell cast
    SpellCastOmni = 16, // Omni-directional spell cast
    Attack1H = 26,      // One-handed attack
    Attack2H = 27,      // Two-handed attack
    SpellChannel = 37,  // Directed spell channel
    SpellChannelOmni = 38, // Omni-directional spell channel
    SwimIdle = 39,      // Idle in water
    Swim = 40,          // Swimming
    Dance = 60,         // Dance emote
    JumpStart = 69,     // Jump takeoff
    Jump = 70,          // Jumping (airborne)
    JumpEnd = 71,       // Jump landing
    Fall = 80           // Falling
};

/**
 * Current animation state for the state machine.
 * Determines which animation should be playing based on entity conditions.
 */
UENUM(BlueprintType)
enum class EWowAnimState : uint8
{
    Idle,           // Standing still
    Walking,        // Moving slowly
    Running,        // Moving at normal speed
    Swimming,       // In water
    SwimmingIdle,   // Idle in water
    Jumping,        // Jump sequence
    Falling,        // Falling through air
    Combat,         // Combat animations
    Casting,        // Spell casting
    Channeling,     // Spell channeling
    Dead            // Death state
};

/**
 * WoW Movement flags for animation state detection.
 * These match the server movement flags from 3.3.5a.
 */
namespace WowMovementFlags
{
    static constexpr uint32 SWIMMING = 0x00200000;
    static constexpr uint32 FLYING = 0x02000000;
    static constexpr uint32 FALLING = 0x00001000;
    static constexpr uint32 FORWARD = 0x00000001;
    static constexpr uint32 BACKWARD = 0x00000002;
    static constexpr uint32 STRAFE_LEFT = 0x00000004;
    static constexpr uint32 STRAFE_RIGHT = 0x00000008;
    static constexpr uint32 TURN_LEFT = 0x00000010;
    static constexpr uint32 TURN_RIGHT = 0x00000020;
    static constexpr uint32 PITCH_UP = 0x00000040;
    static constexpr uint32 PITCH_DOWN = 0x00000080;
    static constexpr uint32 WALK_MODE = 0x00000100;
    static constexpr uint32 JUMPING = 0x00002000;
}

/**
 * Animation controller for WoW character models.
 * Manages animation state machine and switches between M2 animations based on
 * movement state, combat state, and other entity conditions.
 */
UCLASS(BlueprintType)
class WOWASSETS_API UWowAnimationController : public UObject
{
    GENERATED_BODY()

public:
    UWowAnimationController();

    /**
     * Initialize the controller with a skeletal mesh component and available animations.
     * Caches animations by their M2 AnimationId for quick lookup.
     */
    UFUNCTION(BlueprintCallable)
    void Initialize(USkeletalMeshComponent* InMeshComponent, const TArray<UAnimSequence*>& Animations);

    /**
     * Update animation state based on movement info and entity conditions.
     * Call this from Tick() or when movement state changes.
     */
    UFUNCTION(BlueprintCallable)
    void UpdateAnimationState(const struct FWowMovementInfo& MovementInfo, bool bIsInCombat = false, bool bIsCasting = false);

    /**
     * Update animation state for local player based on UE5 character movement.
     * Translates UE5 movement component state to WoW animation state.
     */
    UFUNCTION(BlueprintCallable)
    void UpdateLocalPlayerState(class ACharacter* PlayerCharacter);

    /**
     * Play a specific animation by M2 animation ID.
     * Returns true if animation was found and started.
     */
    UFUNCTION(BlueprintCallable)
    bool PlayAnimationById(EWowAnimId AnimId, bool bLooping = true);

    /**
     * Get current animation state.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure)
    EWowAnimState GetCurrentState() const { return CurrentState; }

    /**
     * Check if the controller has been initialized.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure)
    bool IsInitialized() const { return MeshComponent != nullptr && AnimationCache.Num() > 0; }

protected:
    /** Target skeletal mesh component */
    UPROPERTY()
    TObjectPtr<USkeletalMeshComponent> MeshComponent;

    /** Cache of animations by M2 animation ID */
    UPROPERTY()
    TMap<int32, TObjectPtr<UAnimSequence>> AnimationCache;

    /** Current animation state */
    UPROPERTY(BlueprintReadOnly)
    EWowAnimState CurrentState = EWowAnimState::Idle;

    /** Currently playing animation */
    UPROPERTY(BlueprintReadOnly)
    TObjectPtr<UAnimSequence> CurrentAnimation;

    /** Jump sequence tracking */
    float JumpStartTime = 0.0f;
    bool bInJumpSequence = false;

private:
    /** Determine animation state from movement info */
    EWowAnimState DetermineAnimationState(const struct FWowMovementInfo& MovementInfo, bool bIsInCombat, bool bIsCasting) const;

    /** Determine animation state from local player movement */
    EWowAnimState DetermineLocalPlayerState(class ACharacter* PlayerCharacter) const;

    /** Get the best animation ID for a given state */
    EWowAnimId GetAnimationForState(EWowAnimState State) const;

    /** Switch to a new animation state */
    void SetAnimationState(EWowAnimState NewState);

    /** Check if character is moving based on movement info */
    bool IsMoving(const struct FWowMovementInfo& MovementInfo) const;

    /** Check if character is walking (vs running) based on movement flags and speed */
    bool IsWalking(const struct FWowMovementInfo& MovementInfo) const;
};