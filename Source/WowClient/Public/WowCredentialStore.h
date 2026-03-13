#pragma once
#include "CoreMinimal.h"
#include "WowCredentialStore.generated.h"

USTRUCT(BlueprintType)
struct WOWCLIENT_API FWowCredential
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Alias;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Username;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ServerAddress;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AuthPort = 3724;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsDefault = false;
    FString Password;
};

UCLASS(BlueprintType)
class WOWCLIENT_API UWowCredentialStore : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) bool LoadCredentials();
    UFUNCTION(BlueprintCallable) bool SaveCredentials();
    UFUNCTION(BlueprintCallable) void AddCredential(const FWowCredential& Cred);
    UFUNCTION(BlueprintCallable) void RemoveCredential(const FString& Alias);
    UFUNCTION(BlueprintCallable) FWowCredential GetDefaultCredential() const;
    UFUNCTION(BlueprintCallable) TArray<FWowCredential> GetAllCredentials() const { return Creds; }
private:
    FString GetFilePath() const;
    TArray<FWowCredential> Creds;
};
