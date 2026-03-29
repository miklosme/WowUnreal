#pragma once
#include "CoreMinimal.h"
#include "WowEntity.h"
#include "WowAnimationController.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;
struct FM2AnimationData;

/**
 * WoW M2 Animation IDs from AnimationData.dbc
 * Maps to specific character actions for state machine control.
 */
/**
 * WoW 3.3.5a Animation IDs from AnimationData.dbc.
 * These MUST match the actual DBC IDs — they're used to look up
 * animations from M2 model files via the animation cache.
 */
UENUM(BlueprintType)
enum class EWowAnimId : uint8
{
    Stand = 0,              // Idle standing
    Death = 1,              // Dying animation (plays once)
    Spell = 2,              // Generic spell animation
    Stop = 3,               // Stop moving
    Walk = 4,               // Walking
    Run = 5,                // Running
    Dead = 6,               // Dead pose (looping, after Death)
    Rise = 7,               // Resurrect rise
    StandWound = 8,         // Wound while standing
    CombatWound = 9,        // Wound while in combat
    CombatCritical = 10,    // Critical hit reaction
    ShuffleLeft = 11,       // Strafe left
    ShuffleRight = 12,      // Strafe right
    Walkbackwards = 13,     // Walk backwards
    Stun = 14,              // Stunned
    HandsClosed = 15,       // Hands closed idle
    AttackUnarmed = 16,     // Unarmed attack
    Attack1H = 17,          // One-handed melee attack
    Attack2H = 18,          // Two-handed melee attack
    Attack2HL = 19,         // Two-handed large melee attack
    ParryUnarmed = 20,      // Parry unarmed
    Parry1H = 21,           // Parry one-handed
    Parry2H = 22,           // Parry two-handed
    Parry2HL = 23,          // Parry two-handed large
    ShieldBlock = 24,       // Shield block
    ReadyUnarmed = 25,      // Ready stance unarmed
    Ready1H = 26,           // Ready stance one-handed
    Ready2H = 27,           // Ready stance two-handed
    Ready2HL = 28,          // Ready stance two-handed large
    ReadyBow = 29,          // Ready bow
    Dodge = 30,             // Dodge
    SpellPrecast = 31,      // Spell precast
    SpellCast = 32,         // Spell cast (generic)
    SpellCastArea = 33,     // Area spell cast
    NPCWelcome = 34,        // NPC greeting
    NPCGoodbye = 35,        // NPC farewell
    Block = 36,             // Block
    JumpStart = 37,         // Jump takeoff
    Jump = 38,              // Jumping (airborne)
    JumpEnd = 39,           // Jump landing
    Fall = 40,              // Falling
    SwimIdle = 41,          // Idle in water
    Swim = 42,              // Swimming
    SwimLeft = 43,          // Swim strafe left
    SwimRight = 44,         // Swim strafe right
    SwimBackward = 45,      // Swim backward
    AttackBow = 46,         // Bow attack
    FireBow = 47,           // Fire bow (projectile release)
    ReadyRifle = 48,        // Ready rifle
    AttackRifle = 49,       // Rifle attack
    Loot = 50,              // Looting
    ReadySpellDirected = 51, // Ready directed spell
    ReadySpellOmni = 52,    // Ready omni spell
    SpellCastDirected = 53,  // Directed spell cast
    SpellCastOmni = 54,     // Omni-directional spell cast
    SpellCastDirectedNose = 55, // BattleRoar / ReadyAbility
    Special1H = 57,         // Special one-handed attack
    Special2H = 58,         // Special two-handed attack
    ShieldBash = 59,        // Shield bash
    EmoteTalk = 60,         // Talk emote
    EmoteEat = 61,          // Eat emote
    EmoteWork = 62,         // Work emote
    EmoteUseStanding = 63,  // Use standing emote
    ChannelCast = 65,       // Channel cast directed
    ChannelCastOmni = 66,   // Channel cast omni-directional
    Whirlwind = 67,         // Whirlwind
    Kick = 68,              // Kick
    SitGround = 69,         // Sit on ground
    SitChair = 70,          // Sit on chair
    Sleep = 71,             // Sleeping
    Kneel = 72,             // Kneeling
    Dance = 109,            // Dance emote
    EmoteBow = 115,         // Bow emote
    FlyRun = 147,           // Flying run
    FlyWalk = 148,          // Flying walk
    FlyStand = 149,         // Flying stand
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
     * Play attack animation with fallback chain.
     * Tries Attack1H -> AttackUnarmed -> Attack2H -> Spell
     * Returns true if any animation was successfully played.
     */
    UFUNCTION(BlueprintCallable)
    bool PlayAttackAnimation();

    /**
     * Play wound animation with fallback chain.
     * Tries CombatWound -> StandWound
     * Returns true if any animation was successfully played.
     */
    UFUNCTION(BlueprintCallable)
    bool PlayWoundAnimation();

    /**
     * Get current animation state.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure)
    EWowAnimState GetCurrentState() const { return CurrentState; }

    /**
     * Check if the controller has been initialized.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure)
    bool IsInitialized() const { return MeshComponent != nullptr && AllAnimations.Num() > 0; }

    /**
     * Check if a one-shot animation (attack, wound) is currently playing.
     * State machine updates should be skipped while this returns true.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure)
    bool IsPlayingOneShot() const { return bPlayingOneShot && FPlatformTime::Seconds() < OneShotEndTime; }

    /**
     * Get the animation cache for copying to another controller.
     */
    const TMap<int32, TObjectPtr<UAnimSequence>>& GetAnimationCache() const { return AnimationCache; }

    /** Get all animations (ordered by M2 track index) */
    const TArray<TObjectPtr<UAnimSequence>>& GetAllAnimations() const { return AllAnimations; }

    /** Set the animation ID mapping (M2 animID → track index) */
    void SetAnimationIdMap(const TMap<int32, int32>& InMap);

protected:
    /** Target skeletal mesh component */
    UPROPERTY()
    TObjectPtr<USkeletalMeshComponent> MeshComponent;

    /** All animations ordered by M2 track index */
    UPROPERTY()
    TArray<TObjectPtr<UAnimSequence>> AllAnimations;

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

    /** One-shot animation lock — prevents state machine from overriding non-looping anims */
    float OneShotEndTime = 0.0f;
    bool bPlayingOneShot = false;

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