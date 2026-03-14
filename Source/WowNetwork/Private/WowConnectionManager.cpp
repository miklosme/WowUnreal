#include "WowConnectionManager.h"
#include "Net/WowAuthSocket.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowNet, Log, All);

void UWowConnectionManager::SetState(EWowSessionState S)
{
    State = S;
    OnStateChanged.Broadcast(S);
}

void UWowConnectionManager::Login(const FString& U, const FString& P, const FString& S, int32 Port)
{
    UE_LOG(LogWowNet, Log, TEXT("Login: %s@%s:%d"), *U, *S, Port);

    // Disconnect any existing session
    Disconnect();

    SetState(EWowSessionState::AuthConnecting);

    AuthSocket = MakeShared<FWowAuthSocket>();
    AuthSocket->OnAuthResult.BindUObject(this, &UWowConnectionManager::OnAuthResultReceived);
    AuthSocket->OnRealmList.BindUObject(this, &UWowConnectionManager::OnRealmListReceived);

    if (!AuthSocket->Connect(S, Port, U, P))
    {
        UE_LOG(LogWowNet, Error, TEXT("Failed to connect to auth server"));
        SetState(EWowSessionState::Error);
        OnError.Broadcast(TEXT("Failed to connect to auth server"));
        AuthSocket.Reset();
        return;
    }

    SetState(EWowSessionState::AuthChallengeSent);
}

void UWowConnectionManager::OnAuthResultReceived(bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogWowNet, Log, TEXT("Authentication successful"));
        SessionKey = AuthSocket->GetSessionKey();
        SetState(EWowSessionState::AuthProofSent);

        // Automatically request realm list
        SetState(EWowSessionState::AuthRequestingRealmList);
        AuthSocket->RequestRealmList();
    }
    else
    {
        UE_LOG(LogWowNet, Error, TEXT("Authentication failed"));
        SetState(EWowSessionState::Error);
        OnError.Broadcast(TEXT("Authentication failed"));
        AuthSocket.Reset();
    }
}

void UWowConnectionManager::OnRealmListReceived(const TArray<FWowRealmInfo>& Realms)
{
    UE_LOG(LogWowNet, Log, TEXT("Received %d realms"), Realms.Num());
    CachedRealms = Realms;
    SetState(EWowSessionState::AuthHaveRealmList);
    OnRealmList.Broadcast(Realms);
}

void UWowConnectionManager::SelectRealm(int32 I)
{
    UE_LOG(LogWowNet, Log, TEXT("Realm: %d"), I);

    if (!CachedRealms.IsValidIndex(I))
    {
        UE_LOG(LogWowNet, Error, TEXT("Invalid realm index: %d"), I);
        OnError.Broadcast(TEXT("Invalid realm index"));
        return;
    }

    const FWowRealmInfo& Realm = CachedRealms[I];
    UE_LOG(LogWowNet, Log, TEXT("Selected realm: %s (%s:%d)"), *Realm.Name, *Realm.Address, Realm.Port);

    // World socket connection will be implemented later
    SetState(EWowSessionState::WorldConnecting);
}

void UWowConnectionManager::RequestCharacterList()
{
    UE_LOG(LogWowNet, Log, TEXT("Char list"));
}

void UWowConnectionManager::EnterWorld(uint64 G)
{
    UE_LOG(LogWowNet, Log, TEXT("Enter: %llu"), G);
}

void UWowConnectionManager::Disconnect()
{
    if (AuthSocket.IsValid())
    {
        AuthSocket->Disconnect();
        AuthSocket.Reset();
    }

    SessionKey.Empty();
    CachedRealms.Empty();

    SetState(EWowSessionState::Disconnected);
}
