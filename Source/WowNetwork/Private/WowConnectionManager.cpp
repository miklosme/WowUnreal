#include "WowConnectionManager.h"
#include "Net/WowAuthSocket.h"
#include "Net/WowWorldSocket.h"

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

    CachedAccountName = U;
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

    SetState(EWowSessionState::WorldConnecting);

    // Disconnect auth socket -- no longer needed
    if (AuthSocket.IsValid())
    {
        AuthSocket->Disconnect();
        AuthSocket.Reset();
    }

    // Create and connect world socket
    WorldSocket = MakeShared<FWowWorldSocket>();
    WorldSocket->OnAuthResult.BindUObject(this, &UWowConnectionManager::OnWorldAuthResult);
    WorldSocket->OnCharacterList.BindUObject(this, &UWowConnectionManager::OnWorldCharacterList);

    if (!WorldSocket->Connect(Realm.Address, Realm.Port, CachedAccountName, SessionKey))
    {
        UE_LOG(LogWowNet, Error, TEXT("Failed to connect to world server %s:%d"), *Realm.Address, Realm.Port);
        SetState(EWowSessionState::Error);
        OnError.Broadcast(TEXT("Failed to connect to world server"));
        WorldSocket.Reset();
        return;
    }

    UE_LOG(LogWowNet, Log, TEXT("Connecting to world server %s:%d ..."), *Realm.Address, Realm.Port);
}

void UWowConnectionManager::OnWorldAuthResult(bool bSuccess)
{
    if (bSuccess)
    {
        UE_LOG(LogWowNet, Log, TEXT("World server authenticated"));
        SetState(EWowSessionState::WorldAuthenticated);
    }
    else
    {
        UE_LOG(LogWowNet, Error, TEXT("World server authentication failed"));
        SetState(EWowSessionState::Error);
        OnError.Broadcast(TEXT("World server authentication failed"));
        WorldSocket.Reset();
    }
}

void UWowConnectionManager::OnWorldCharacterList(const TArray<FWowCharacterInfo>& Characters)
{
    UE_LOG(LogWowNet, Log, TEXT("Received %d characters from world server"), Characters.Num());
    CachedCharacters = Characters;
    SetState(EWowSessionState::WorldHaveCharList);
    OnCharacterList.Broadcast(Characters);
}

void UWowConnectionManager::RequestCharacterList()
{
    UE_LOG(LogWowNet, Log, TEXT("Requesting character list"));

    if (!WorldSocket.IsValid())
    {
        UE_LOG(LogWowNet, Error, TEXT("No world socket connection"));
        OnError.Broadcast(TEXT("Not connected to world server"));
        return;
    }

    SetState(EWowSessionState::WorldRequestingCharList);
    WorldSocket->SendCharEnum();
}

void UWowConnectionManager::EnterWorld(int64 G)
{
    UE_LOG(LogWowNet, Log, TEXT("Enter world: GUID %llu"), G);

    if (!WorldSocket.IsValid())
    {
        UE_LOG(LogWowNet, Error, TEXT("No world socket connection"));
        OnError.Broadcast(TEXT("Not connected to world server"));
        return;
    }

    SetState(EWowSessionState::WorldEnteringWorld);
    WorldSocket->SendPlayerLogin(G);
}

void UWowConnectionManager::Disconnect()
{
    if (WorldSocket.IsValid())
    {
        WorldSocket->Disconnect();
        WorldSocket.Reset();
    }

    if (AuthSocket.IsValid())
    {
        AuthSocket->Disconnect();
        AuthSocket.Reset();
    }

    SessionKey.Empty();
    CachedRealms.Empty();
    CachedCharacters.Empty();
    CachedAccountName.Empty();

    SetState(EWowSessionState::Disconnected);
}
