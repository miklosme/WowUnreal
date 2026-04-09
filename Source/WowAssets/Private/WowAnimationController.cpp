#include "WowAnimationController.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "WowNetwork/Public/WowEntity.h"
#include "WowData/Public/Formats/M2Types.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowAnim, Log, All);

UWowAnimationController::UWowAnimationController()
{
}

void UWowAnimationController::Initialize(USkeletalMeshComponent* InMeshComponent, const TArray<UAnimSequence*>& Animations)
{
    MeshComponent = InMeshComponent;
    AnimationCache.Empty();

    if (!MeshComponent)
    {
        UE_LOG(LogWowAnim, Error, TEXT("WowAnimationController: Cannot initialize without valid skeletal mesh component"));
        return;
    }

    // Store all animations by track index
    AllAnimations.Empty();
    for (UAnimSequence* Anim : Animations)
    {
        AllAnimations.Add(Anim);
    }

    UE_LOG(LogWowAnim, Log, TEXT("WowAnimationController initialized with %d animations"), AllAnimations.Num());

    // Play idle animation immediately - try mapped idle first, then fallback
    if (MeshComponent)
    {
        UAnimSequence* IdleAnim = nullptr;

        // Try to find mapped idle animation (ID 0)
        if (TObjectPtr<UAnimSequence>* Found = AnimationCache.Find(0))
        {
            IdleAnim = *Found;
            UE_LOG(LogWowAnim, Log, TEXT("WowAnimationController::Initialize: Found mapped Stand animation (ID 0): %s"),
                IdleAnim ? *IdleAnim->GetName() : TEXT("NULL"));
        }

        // Fallback to first animation if no mapped idle found
        if (!IdleAnim && AllAnimations.Num() > 0 && AllAnimations[0])
        {
            IdleAnim = AllAnimations[0];
            UE_LOG(LogWowAnim, Warning, TEXT("WowAnimationController::Initialize: No Stand animation (ID 0) found, falling back to AllAnimations[0]: %s"),
                IdleAnim ? *IdleAnim->GetName() : TEXT("NULL"));
        }

        if (IdleAnim)
        {
            MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
            MeshComponent->PlayAnimation(IdleAnim, true);
            UE_LOG(LogWowAnim, Log, TEXT("WowAnimationController::Initialize: Playing initial animation: %s"), *IdleAnim->GetName());
        }
        else
        {
            UE_LOG(LogWowAnim, Error, TEXT("WowAnimationController::Initialize: No animations available to play"));
        }
    }

    // Start with idle animation
    SetAnimationState(EWowAnimState::Idle);
}

void UWowAnimationController::UpdateAnimationState(const FWowMovementInfo& MovementInfo, bool bIsInCombat, bool bIsCasting)
{
    if (!IsInitialized())
    {
        return;
    }

    // Don't override one-shot animations (attack, wound) until they finish
    if (IsPlayingOneShot())
    {
        return;
    }
    bPlayingOneShot = false;

    EWowAnimState NewState = DetermineAnimationState(MovementInfo, bIsInCombat, bIsCasting);

    // Handle jump sequence timing
    if (CurrentState == EWowAnimState::Jumping && bInJumpSequence)
    {
        float CurrentTime = FPlatformTime::Seconds();
        float JumpDuration = CurrentTime - JumpStartTime;

        // Switch from jump start to airborne after brief takeoff
        if (JumpDuration > 0.2f && NewState == EWowAnimState::Jumping)
        {
            if (MovementInfo.MoveFlags & WowMovementFlags::FALLING)
            {
                NewState = EWowAnimState::Falling;
            }
        }

        // End jump sequence if landed
        if (!(MovementInfo.MoveFlags & (WowMovementFlags::JUMPING | WowMovementFlags::FALLING)))
        {
            bInJumpSequence = false;
            // Transition to appropriate movement state
            NewState = DetermineAnimationState(MovementInfo, bIsInCombat, bIsCasting);
        }
    }

    if (NewState != CurrentState)
    {
        SetAnimationState(NewState);
    }
}

void UWowAnimationController::UpdateLocalPlayerState(ACharacter* PlayerCharacter)
{
    if (!IsInitialized() || !PlayerCharacter)
    {
        return;
    }

    // Don't override one-shot animations (attack, wound) until they finish
    if (IsPlayingOneShot())
    {
        return;
    }
    bPlayingOneShot = false;

    EWowAnimState NewState = DetermineLocalPlayerState(PlayerCharacter);

    // Handle jump sequence for local player
    UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement();
    if (Movement)
    {
        if (Movement->IsFalling() && CurrentState != EWowAnimState::Falling)
        {
            if (Movement->Velocity.Z > 0)
            {
                // Starting a jump
                NewState = EWowAnimState::Jumping;
                bInJumpSequence = true;
                JumpStartTime = FPlatformTime::Seconds();
            }
            else
            {
                // Just falling
                NewState = EWowAnimState::Falling;
            }
        }
        else if (!Movement->IsFalling() && bInJumpSequence)
        {
            // Landed
            bInJumpSequence = false;
            NewState = DetermineLocalPlayerState(PlayerCharacter);
        }
    }

    if (NewState != CurrentState)
    {
        SetAnimationState(NewState);
    }
}

// NOTE: PlayAnimationById defined at end of file (needs AnimationCache + AllAnimations)

EWowAnimState UWowAnimationController::DetermineAnimationState(const FWowMovementInfo& MovementInfo, bool bIsInCombat, bool bIsCasting) const
{
    // Priority: casting/channeling > combat > movement > idle

    if (bIsCasting)
    {
        return EWowAnimState::Casting;
    }

    // Check for swimming
    if (MovementInfo.MoveFlags & WowMovementFlags::SWIMMING)
    {
        if (IsMoving(MovementInfo))
        {
            return EWowAnimState::Swimming;
        }
        else
        {
            return EWowAnimState::SwimmingIdle;
        }
    }

    // Check for jumping/falling
    if (MovementInfo.MoveFlags & WowMovementFlags::JUMPING)
    {
        return EWowAnimState::Jumping;
    }

    if (MovementInfo.MoveFlags & WowMovementFlags::FALLING)
    {
        return EWowAnimState::Falling;
    }

    // Check for movement
    if (IsMoving(MovementInfo))
    {
        if (bIsInCombat)
        {
            return EWowAnimState::Combat;
        }
        else if (IsWalking(MovementInfo))
        {
            return EWowAnimState::Walking;
        }
        else
        {
            return EWowAnimState::Running;
        }
    }

    // Default idle state
    return EWowAnimState::Idle;
}

EWowAnimState UWowAnimationController::DetermineLocalPlayerState(ACharacter* PlayerCharacter) const
{
    if (!PlayerCharacter)
    {
        return EWowAnimState::Idle;
    }

    UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement();
    if (!Movement)
    {
        return EWowAnimState::Idle;
    }

    // Check if swimming
    if (Movement->IsSwimming())
    {
        FVector Velocity = Movement->Velocity;
        if (Velocity.Size2D() > 10.0f) // Moving threshold
        {
            return EWowAnimState::Swimming;
        }
        else
        {
            return EWowAnimState::SwimmingIdle;
        }
    }

    // Check if falling
    if (Movement->IsFalling())
    {
        if (Movement->Velocity.Z > 0)
        {
            return EWowAnimState::Jumping;
        }
        else
        {
            return EWowAnimState::Falling;
        }
    }

    // Check movement
    FVector Velocity = Movement->Velocity;
    float Speed = Velocity.Size2D();

    if (Speed > 10.0f) // Moving threshold
    {
        // Check if walking (based on character's walking flag)
        if (Movement->GetMaxSpeed() <= Movement->MaxWalkSpeed * 1.1f) // Allow small tolerance
        {
            return EWowAnimState::Walking;
        }
        else
        {
            return EWowAnimState::Running;
        }
    }

    return EWowAnimState::Idle;
}

EWowAnimId UWowAnimationController::GetAnimationForState(EWowAnimState State) const
{
    switch (State)
    {
        case EWowAnimState::Idle:
            return EWowAnimId::Stand;
        case EWowAnimState::Walking:
            return EWowAnimId::Walk;
        case EWowAnimState::Running:
            return EWowAnimId::Run;
        case EWowAnimState::Swimming:
            return EWowAnimId::Swim;
        case EWowAnimState::SwimmingIdle:
            return EWowAnimId::SwimIdle;
        case EWowAnimState::Jumping:
            return EWowAnimId::JumpStart;
        case EWowAnimState::Falling:
            return EWowAnimId::Fall;
        case EWowAnimState::Combat:
            return EWowAnimId::ReadyUnarmed;
        case EWowAnimState::Casting:
            return EWowAnimId::SpellCastDirected;
        case EWowAnimState::Channeling:
            return EWowAnimId::ChannelCast;
        case EWowAnimState::Dead:
            return EWowAnimId::Death;
        default:
            return EWowAnimId::Stand;
    }
}

void UWowAnimationController::SetAnimationState(EWowAnimState NewState)
{
    if (NewState == CurrentState)
    {
        return;
    }

    // State machine transitions always clear one-shot lock — one-shots are only
    // for explicit combat animations (attack/wound) triggered outside the state machine
    bPlayingOneShot = false;

    CurrentState = NewState;
    EWowAnimId AnimId = GetAnimationForState(NewState);

    // Determine if animation should loop
    bool bShouldLoop = true;
    switch (NewState)
    {
        case EWowAnimState::Jumping:
        case EWowAnimState::Dead:
            bShouldLoop = false;
            break;
        default:
            bShouldLoop = true;
            break;
    }

    if (PlayAnimationById(AnimId, bShouldLoop))
    {
        UE_LOG(LogWowAnim, Log, TEXT("Animation state changed to %s (AnimID %d)"),
            *UEnum::GetValueAsString(CurrentState), static_cast<int32>(AnimId));
    }
    else
    {
        UE_LOG(LogWowAnim, Warning, TEXT("Failed to play animation for state %s (AnimID %d)"),
            *UEnum::GetValueAsString(CurrentState), static_cast<int32>(AnimId));
    }
}

bool UWowAnimationController::IsMoving(const FWowMovementInfo& MovementInfo) const
{
    return (MovementInfo.MoveFlags & (
        WowMovementFlags::FORWARD |
        WowMovementFlags::BACKWARD |
        WowMovementFlags::STRAFE_LEFT |
        WowMovementFlags::STRAFE_RIGHT
    )) != 0;
}

bool UWowAnimationController::IsWalking(const FWowMovementInfo& MovementInfo) const
{
    return (MovementInfo.MoveFlags & WowMovementFlags::WALK_MODE) != 0;
}
void UWowAnimationController::SetAnimationIdMap(const TMap<int32, int32>& InMap)
{
    // Build AnimationCache from the map: M2 AnimId -> UAnimSequence
    AnimationCache.Empty();
    for (const auto& Pair : InMap)
    {
        int32 AnimId = Pair.Key;
        int32 TrackIndex = Pair.Value;
        if (AllAnimations.IsValidIndex(TrackIndex) && AllAnimations[TrackIndex])
        {
            AnimationCache.Add(AnimId, AllAnimations[TrackIndex]);
            UE_LOG(LogWowAnim, Log, TEXT("SetAnimationIdMap: Mapped AnimID %d -> TrackIndex %d -> Animation %s"),
                AnimId, TrackIndex, *AllAnimations[TrackIndex]->GetName());
        }
        else
        {
            UE_LOG(LogWowAnim, Warning, TEXT("SetAnimationIdMap: Invalid TrackIndex %d for AnimID %d"), TrackIndex, AnimId);
        }
    }
    UE_LOG(LogWowAnim, Log, TEXT("SetAnimationIdMap: mapped %d animation IDs"), AnimationCache.Num());

    // Play idle if available - try alternatives to AnimID 0 if it might be problematic
    UAnimSequence* BestIdleAnim = nullptr;
    int32 BestIdleAnimId = 0;

    // For certain character models, AnimID 0 might not be the correct Stand animation
    // Check if we can find better alternatives first
    bool bTriedAlternatives = false;
    if (AnimationCache.Contains(0))
    {
        // Try alternative stand animations that are commonly used instead of AnimID 0
        TArray<int32> StandFallbackIds = {3, 25}; // Stop (3), ReadyUnarmed (25)
        for (int32 FallbackId : StandFallbackIds)
        {
            if (AnimationCache.Contains(FallbackId))
            {
                BestIdleAnim = AnimationCache[FallbackId];
                BestIdleAnimId = FallbackId;
                bTriedAlternatives = true;
                UE_LOG(LogWowAnim, Log, TEXT("SetAnimationIdMap: Using AnimID %d as Stand animation (alternative to potentially problematic AnimID 0)"), FallbackId);
                break;
            }
        }

        // If no alternatives found, use AnimID 0
        if (!BestIdleAnim)
        {
            BestIdleAnim = AnimationCache[0];
            BestIdleAnimId = 0;
            UE_LOG(LogWowAnim, Log, TEXT("SetAnimationIdMap: Using AnimID 0 as Stand animation (no alternatives available)"));
        }
    }

    if (BestIdleAnim && MeshComponent)
    {
        MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        MeshComponent->PlayAnimation(BestIdleAnim, true);
        CurrentAnimation = BestIdleAnim;
        UE_LOG(LogWowAnim, Log, TEXT("SetAnimationIdMap: Playing Stand animation (ID %d): %s"), BestIdleAnimId, *BestIdleAnim->GetName());
    }
    else if (MeshComponent)
    {
        UE_LOG(LogWowAnim, Warning, TEXT("SetAnimationIdMap: No Stand animation available in AnimationCache"));
    }
}

bool UWowAnimationController::PlayAnimationById(EWowAnimId AnimId, bool bLooping)
{
    if (!MeshComponent) return false;

    int32 Id = static_cast<int32>(AnimId);
    UAnimSequence* AnimToPlay = nullptr;

    // Special handling for Stand animation - AnimID 0 might be incorrect in some models
    if (Id == 0) // Stand animation request
    {
        // First try the mapped AnimID 0
        if (TObjectPtr<UAnimSequence>* Found = AnimationCache.Find(0))
        {
            if (*Found)
            {
                AnimToPlay = *Found;
                // Check if this might be a problematic animation by name
                FString AnimName = (*Found)->GetName();
                if (AnimName.Contains(TEXT("kneel"), ESearchCase::IgnoreCase) ||
                    AnimName.Contains(TEXT("sit"), ESearchCase::IgnoreCase))
                {
                    UE_LOG(LogWowAnim, Warning, TEXT("PlayAnimationById: AnimID 0 appears to be '%s', trying alternatives"), *AnimName);
                    AnimToPlay = nullptr; // Force fallback
                }
            }
        }

        // If AnimID 0 is not available or seems wrong, try alternative stand animations
        if (!AnimToPlay)
        {
            TArray<int32> StandFallbackIds = {3, 25}; // Stop (3) is sometimes used as stand, ReadyUnarmed (25) might work
            for (int32 FallbackId : StandFallbackIds)
            {
                if (TObjectPtr<UAnimSequence>* FallbackFound = AnimationCache.Find(FallbackId))
                {
                    if (*FallbackFound)
                    {
                        AnimToPlay = *FallbackFound;
                        UE_LOG(LogWowAnim, Log, TEXT("PlayAnimationById: Using AnimID %d as Stand animation fallback"), FallbackId);
                        break;
                    }
                }
            }
        }

        // Last resort: use the first animation in the array
        if (!AnimToPlay && AllAnimations.Num() > 0 && AllAnimations[0])
        {
            AnimToPlay = AllAnimations[0];
            UE_LOG(LogWowAnim, Warning, TEXT("PlayAnimationById: Using AllAnimations[0] as final Stand fallback"));
        }
    }
    else
    {
        // For non-Stand animations, use normal lookup
        if (TObjectPtr<UAnimSequence>* Found = AnimationCache.Find(Id))
        {
            if (*Found)
            {
                AnimToPlay = *Found;
            }
        }
    }

    // Fallback chain for missing animations
    if (!AnimToPlay)
    {
        TArray<int32> FallbackIds;

        switch (Id)
        {
            case 4: // Walk
                FallbackIds = {0}; // Stand
                break;
            case 5: // Run
                FallbackIds = {4, 0}; // Walk → Stand
                break;
            case 33: // CombatWound
                FallbackIds = {0}; // Stand
                break;
            case 51: case 52: case 53: case 54: // SpellCast variants
                FallbackIds = {0}; // Stand
                break;
            case 16: case 17: case 18: case 19: // Attack animations
            case 57: case 58: case 59: case 68: // Special attacks
                FallbackIds = {0}; // Stand
                break;
            default:
                if (Id != 0) // Any unknown animation
                {
                    FallbackIds = {0}; // Stand
                }
                break;
        }

        // Try fallback animations
        for (int32 FallbackId : FallbackIds)
        {
            if (TObjectPtr<UAnimSequence>* FallbackFound = AnimationCache.Find(FallbackId))
            {
                if (*FallbackFound)
                {
                    AnimToPlay = *FallbackFound;
                    break;
                }
            }
        }
    }

    if (!AnimToPlay) return false;

    MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    MeshComponent->PlayAnimation(AnimToPlay, bLooping);
    CurrentAnimation = AnimToPlay;

    // Track one-shot animations so state machine doesn't override them
    if (!bLooping)
    {
        bPlayingOneShot = true;
        float AnimDuration = AnimToPlay->GetPlayLength();
        OneShotEndTime = FPlatformTime::Seconds() + AnimDuration;
    }
    else
    {
        bPlayingOneShot = false;
    }

    return true;
}

bool UWowAnimationController::PlayAttackAnimation()
{
    // Try attack animations in order of preference until one succeeds
    if (PlayAnimationById(EWowAnimId::Attack1H, false)) return true;
    if (PlayAnimationById(EWowAnimId::AttackUnarmed, false)) return true;
    if (PlayAnimationById(EWowAnimId::Attack2H, false)) return true;
    if (PlayAnimationById(EWowAnimId::Spell, false)) return true;

    UE_LOG(LogWowAnim, Warning, TEXT("PlayAttackAnimation: No attack animations available"));
    return false;
}

bool UWowAnimationController::PlayWoundAnimation()
{
    // Try wound animations in order of preference until one succeeds
    if (PlayAnimationById(EWowAnimId::CombatWound, false)) return true;
    if (PlayAnimationById(EWowAnimId::StandWound, false)) return true;

    UE_LOG(LogWowAnim, Warning, TEXT("PlayWoundAnimation: No wound animations available"));
    return false;
}
