#pragma once
#include "CoreMinimal.h"
#include "WowSessionState.generated.h"

UENUM(BlueprintType)
enum class EWowSessionState : uint8
{
    Disconnected,
    AuthConnecting, AuthChallengeSent, AuthProofSent, AuthRequestingRealmList, AuthHaveRealmList,
    WorldConnecting, WorldAuthChallengeReceived, WorldAuthSessionSent, WorldAuthenticated,
    WorldRequestingCharList, WorldHaveCharList, WorldEnteringWorld, WorldInGame,
    Error
};

USTRUCT(BlueprintType)
struct WOWNETWORK_API FWowRealmInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString Address;
    UPROPERTY(BlueprintReadOnly) int32 Port = 0;
    UPROPERTY(BlueprintReadOnly) int32 CharacterCount = 0;
    UPROPERTY(BlueprintReadOnly) uint8 Type = 0;
};

USTRUCT(BlueprintType)
struct WOWNETWORK_API FWowCharacterInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int64 Guid = 0;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) uint8 Race = 0;
    UPROPERTY(BlueprintReadOnly) uint8 Class = 0;
    UPROPERTY(BlueprintReadOnly) uint8 Gender = 0;
    UPROPERTY(BlueprintReadOnly) uint8 Level = 0;
    UPROPERTY(BlueprintReadOnly) int32 MapId = 0;
    UPROPERTY(BlueprintReadOnly) int32 ZoneId = 0;
    UPROPERTY(BlueprintReadOnly) FVector Position = FVector::ZeroVector;
};
