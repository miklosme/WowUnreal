#include "WowCredentialStore.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowCred, Log, All);

FString UWowCredentialStore::GetFilePath() const { return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("WowCredentials.json")); }

/** Simple XOR obfuscation + Base64 to prevent plaintext password storage.
 *  NOT cryptographically secure — just prevents casual exposure in JSON. */
static FString ObfuscatePassword(const FString& Plain)
{
	TArray<uint8> Bytes;
	Bytes.SetNum(Plain.Len());
	for (int32 i = 0; i < Plain.Len(); ++i)
	{
		Bytes[i] = static_cast<uint8>(Plain[i] ^ 0xA5 ^ (i & 0xFF));
	}
	return FBase64::Encode(Bytes);
}

static FString DeobfuscatePassword(const FString& Encoded)
{
	TArray<uint8> Bytes;
	if (!FBase64::Decode(Encoded, Bytes)) return FString();
	FString Result;
	Result.Reserve(Bytes.Num());
	for (int32 i = 0; i < Bytes.Num(); ++i)
	{
		Result.AppendChar(static_cast<TCHAR>(Bytes[i] ^ 0xA5 ^ (i & 0xFF)));
	}
	return Result;
}

bool UWowCredentialStore::LoadCredentials()
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *GetFilePath())) return false;
	TSharedPtr<FJsonValue> V;
	TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(R, V) || !V.IsValid()) return false;

	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (!V->TryGetArray(Arr) || !Arr) return false;

	Creds.Empty();
	for (const TSharedPtr<FJsonValue>& I : *Arr)
	{
		const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
		if (!I->TryGetObject(ObjPtr) || !ObjPtr || !ObjPtr->IsValid()) continue;
		const TSharedPtr<FJsonObject>& O = *ObjPtr;

		FWowCredential C;
		O->TryGetStringField(TEXT("alias"), C.Alias);
		O->TryGetStringField(TEXT("username"), C.Username);
		O->TryGetStringField(TEXT("server"), C.ServerAddress);
		O->TryGetNumberField(TEXT("port"), C.AuthPort);
		O->TryGetBoolField(TEXT("default"), C.bIsDefault);

		FString EncodedPw;
		if (O->TryGetStringField(TEXT("pw"), EncodedPw))
		{
			C.Password = DeobfuscatePassword(EncodedPw);
		}
		else if (O->TryGetStringField(TEXT("password"), EncodedPw))
		{
			// Legacy plaintext fallback — will be saved obfuscated on next save
			C.Password = EncodedPw;
			UE_LOG(LogWowCred, Warning, TEXT("Credential '%s' has plaintext password, will be obfuscated on next save"), *C.Alias);
		}

		Creds.Add(C);
	}
	return true;
}

bool UWowCredentialStore::SaveCredentials()
{
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FWowCredential& C : Creds)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("alias"), C.Alias);
		O->SetStringField(TEXT("username"), C.Username);
		O->SetStringField(TEXT("server"), C.ServerAddress);
		O->SetNumberField(TEXT("port"), C.AuthPort);
		O->SetBoolField(TEXT("default"), C.bIsDefault);
		O->SetStringField(TEXT("pw"), ObfuscatePassword(C.Password));
		Arr.Add(MakeShared<FJsonValueObject>(O));
	}
	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Arr, W);
	return FFileHelper::SaveStringToFile(Out, *GetFilePath());
}

void UWowCredentialStore::AddCredential(const FWowCredential& C)
{
	for (FWowCredential& E : Creds) { if (E.Alias == C.Alias) { E = C; return; } }
	Creds.Add(C);
}

void UWowCredentialStore::RemoveCredential(const FString& A) { Creds.RemoveAll([&](const FWowCredential& C){ return C.Alias == A; }); }

FWowCredential UWowCredentialStore::GetDefaultCredential() const
{
	for (const FWowCredential& C : Creds) if (C.bIsDefault) return C;
	return Creds.Num() > 0 ? Creds[0] : FWowCredential();
}
