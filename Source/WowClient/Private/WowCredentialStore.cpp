#include "WowCredentialStore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"

FString UWowCredentialStore::GetFilePath() const { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("WowCredentials.json")); }

bool UWowCredentialStore::LoadCredentials()
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *GetFilePath())) return false;
    TSharedPtr<FJsonValue> V; auto R = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(R, V)) return false;
    Creds.Empty();
    for (auto& I : V->AsArray())
    {
        auto O = I->AsObject(); FWowCredential C;
        C.Alias = O->GetStringField(TEXT("alias"));
        C.Username = O->GetStringField(TEXT("username"));
        C.ServerAddress = O->GetStringField(TEXT("server"));
        C.AuthPort = O->GetIntegerField(TEXT("port"));
        C.bIsDefault = O->GetBoolField(TEXT("default"));
        C.Password = O->GetStringField(TEXT("password"));
        Creds.Add(C);
    }
    return true;
}

bool UWowCredentialStore::SaveCredentials()
{
    TArray<TSharedPtr<FJsonValue>> Arr;
    for (auto& C : Creds)
    {
        TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("alias"), C.Alias);
        O->SetStringField(TEXT("username"), C.Username);
        O->SetStringField(TEXT("server"), C.ServerAddress);
        O->SetNumberField(TEXT("port"), C.AuthPort);
        O->SetBoolField(TEXT("default"), C.bIsDefault);
        O->SetStringField(TEXT("password"), C.Password);
        Arr.Add(MakeShared<FJsonValueObject>(O));
    }
    FString Out; auto W = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Arr, W);
    return FFileHelper::SaveStringToFile(Out, *GetFilePath());
}

void UWowCredentialStore::AddCredential(const FWowCredential& C)
{
    for (auto& E : Creds) { if (E.Alias == C.Alias) { E = C; return; } }
    Creds.Add(C);
}

void UWowCredentialStore::RemoveCredential(const FString& A) { Creds.RemoveAll([&](const FWowCredential& C){ return C.Alias == A; }); }

FWowCredential UWowCredentialStore::GetDefaultCredential() const
{
    for (auto& C : Creds) if (C.bIsDefault) return C;
    return Creds.Num() > 0 ? Creds[0] : FWowCredential();
}
