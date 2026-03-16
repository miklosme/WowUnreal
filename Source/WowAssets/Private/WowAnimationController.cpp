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

    // Cache animations by their M2 animation ID
    // Animation names follow pattern: "ModelName_Anim{AnimId}_{SubAnimId}"
    for (UAnimSequence* Anim : Animations)
    {
        if (!Anim) continue;

        FString AnimName = Anim->GetName();

        // Parse animation ID from name pattern
        int32 AnimPos = AnimName.Find(TEXT("_Anim"));
        if (AnimPos != INDEX_NONE)
        {
            FString IdPart = AnimName.Mid(AnimPos + 5); // Skip "_Anim"
            int32 UnderscorePos = IdPart.Find(TEXT("_"));
            if (UnderscorePos != INDEX_NONE)
            {
                FString AnimIdStr = IdPart.Left(UnderscorePos);
                int32 AnimId = FCString::Atoi(*AnimIdStr);

                if (AnimId >= 0)
                {
                    AnimationCache.Add(AnimId, Anim);
                    UE_LOG(LogWowAnim, Log, TEXT("Cached animation ID %d: %s"), AnimId, *AnimName);
                }
            }
        }
    }

    UE_LOG(LogWowAnim, Log, TEXT("WowAnimationController initialized with %d animations"), AnimationCache.Num());

    // Start with idle animation
    SetAnimationState(EWowAnimState::Idle);
}

void UWowAnimationController::UpdateAnimationState(const FWowMovementInfo& MovementInfo, bool bIsInCombat, bool bIsCasting)
{
    if (!IsInitialized())
    {
        return;
    }

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

bool UWowAnimationController::PlayAnimationById(EWowAnimId AnimId, bool bLooping)
{
    if (!IsInitialized())
    {
        return false;
    }

    TObjectPtr<UAnimSequence>* AnimPtr = AnimationCache.Find(static_cast<int32>(AnimId));
    if (!AnimPtr || !*AnimPtr)
    {
        UE_LOG(LogWowAnim, Warning, TEXT("Animation ID %d not found in cache"), static_cast<int32>(AnimId));
        return false;
    }

    UAnimSequence* Anim = *AnimPtr;
    MeshComponent->PlayAnimation(Anim, bLooping);
    CurrentAnimation = Anim;

    UE_LOG(LogWowAnim, Log, TEXT("Playing animation ID %d: %s"), static_cast<int32>(AnimId), *Anim->GetName());
    return true;
}

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
            return EWowAnimId::Run; // Use run animation for combat movement
        case EWowAnimState::Casting:
            return EWowAnimId::SpellCast;
        case EWowAnimState::Channeling:
            return EWowAnimId::SpellChannel;
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