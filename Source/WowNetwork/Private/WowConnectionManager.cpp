#include "WowConnectionManager.h"
DEFINE_LOG_CATEGORY_STATIC(LogWowNet, Log, All);
void UWowConnectionManager::SetState(EWowSessionState S) { State = S; OnStateChanged.Broadcast(S); }
void UWowConnectionManager::Login(const FString& U, const FString& P, const FString& S, int32 Port)
{ UE_LOG(LogWowNet, Log, TEXT("Login: %s@%s:%d"), *U, *S, Port); SetState(EWowSessionState::AuthConnecting); }
void UWowConnectionManager::SelectRealm(int32 I) { UE_LOG(LogWowNet, Log, TEXT("Realm: %d"), I); }
void UWowConnectionManager::RequestCharacterList() { UE_LOG(LogWowNet, Log, TEXT("Char list")); }
void UWowConnectionManager::EnterWorld(uint64 G) { UE_LOG(LogWowNet, Log, TEXT("Enter: %llu"), G); }
void UWowConnectionManager::Disconnect() { SetState(EWowSessionState::Disconnected); }
