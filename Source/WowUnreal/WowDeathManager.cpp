#include "WowDeathManager.h"
#include "SWowDeathScreen.h"
#include "WowConnectionManager.h"
#include "WowPacketHandler.h"
#include "WowOpcodes.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SWidget.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowDeath, Log, All);

UWowDeathManager::UWowDeathManager()
{
    bIsPlayerDead = false;
}

void UWowDeathManager::Initialize(UWowConnectionManager* ConnectionMgr, TSharedPtr<SWidget> RootWidget)
{
    ConnectionManager = ConnectionMgr;
    RootWidgetPtr = RootWidget;

    if (!ConnectionManager)
    {
        UE_LOG(LogWowDeath, Error, TEXT("Failed to initialize WowDeathManager: ConnectionManager is null"));
        return;
    }

    // Bind to death events from packet handler
    ConnectionManager->PacketHandler.OnPlayerDeath.AddUObject(this, &UWowDeathManager::OnPlayerDeath);
    ConnectionManager->PacketHandler.OnCorpseReclaimDelay.AddUObject(this, &UWowDeathManager::OnCorpseReclaimDelay);
    ConnectionManager->PacketHandler.OnResurrectRequest.AddUObject(this, &UWowDeathManager::OnResurrectRequest);
    ConnectionManager->PacketHandler.OnPlayerTeleport.AddUObject(this, &UWowDeathManager::OnPlayerTeleport);

    UE_LOG(LogWowDeath, Log, TEXT("WowDeathManager initialized"));
}

void UWowDeathManager::Shutdown()
{
    HideDeathScreen();

    if (ConnectionManager)
    {
        ConnectionManager->PacketHandler.OnPlayerDeath.RemoveAll(this);
        ConnectionManager->PacketHandler.OnCorpseReclaimDelay.RemoveAll(this);
        ConnectionManager->PacketHandler.OnResurrectRequest.RemoveAll(this);
        ConnectionManager->PacketHandler.OnPlayerTeleport.RemoveAll(this);
    }

    ConnectionManager = nullptr;
    RootWidgetPtr.Reset();

    UE_LOG(LogWowDeath, Log, TEXT("WowDeathManager shut down"));
}

void UWowDeathManager::ShowDeathScreen()
{
    if (DeathScreenWidget.IsValid() || !RootWidgetPtr.IsValid())
    {
        return; // Already showing or no root widget
    }

    // Create death screen widget
    DeathScreenWidget = SNew(SWowDeathScreen)
        .OnReleaseSpirit(FOnReleaseSpirit::CreateUObject(this, &UWowDeathManager::OnReleaseSpirit));

    // Add to root as overlay
    if (TSharedPtr<SWidget> Root = RootWidgetPtr.Pin())
    {
        // Try to find an overlay in the root widget to add our death screen to
        // For now, we'll try to add it directly - in a real implementation this would
        // need more sophisticated widget tree navigation
        if (TSharedPtr<SOverlay> RootOverlay = StaticCastSharedPtr<SOverlay>(Root))
        {
            RootOverlay->AddSlot()
            .HAlign(HAlign_Fill)
            .VAlign(VAlign_Fill)
            [
                DeathScreenWidget.ToSharedRef()
            ];
        }
    }

    bIsPlayerDead = true;

    UE_LOG(LogWowDeath, Log, TEXT("Death screen shown"));
}

void UWowDeathManager::HideDeathScreen()
{
    if (!DeathScreenWidget.IsValid())
    {
        return; // Not showing
    }

    // Remove from parent
    if (TSharedPtr<SWidget> Parent = DeathScreenWidget->GetParentWidget())
    {
        if (TSharedPtr<SOverlay> ParentOverlay = StaticCastSharedPtr<SOverlay>(Parent))
        {
            ParentOverlay->RemoveSlot(DeathScreenWidget.ToSharedRef());
        }
    }

    DeathScreenWidget.Reset();
    bIsPlayerDead = false;

    UE_LOG(LogWowDeath, Log, TEXT("Death screen hidden"));
}

void UWowDeathManager::OnReleaseSpirit()
{
    if (!ConnectionManager)
    {
        UE_LOG(LogWowDeath, Error, TEXT("Cannot release spirit: ConnectionManager is null"));
        return;
    }

    // Send CMSG_REPOP_REQUEST
    TArray<uint8> Data; // Empty packet
    ConnectionManager->SendRawPacket(WowOpcode::CMSG_REPOP_REQUEST, Data);

    UE_LOG(LogWowDeath, Log, TEXT("Sent CMSG_REPOP_REQUEST"));
}

void UWowDeathManager::OnPlayerDeath()
{
    UE_LOG(LogWowDeath, Warning, TEXT("Player has died!"));
    ShowDeathScreen();
    // TODO: Disable movement input, apply post-process effect
}

void UWowDeathManager::OnCorpseReclaimDelay(float DelayInSeconds)
{
    UE_LOG(LogWowDeath, Log, TEXT("Corpse reclaim delay: %.1f seconds"), DelayInSeconds);

    if (DeathScreenWidget.IsValid())
    {
        DeathScreenWidget->SetCorpseReclaimTimer(DelayInSeconds);
    }
}

void UWowDeathManager::OnResurrectRequest(const FString& RequesterName)
{
    UE_LOG(LogWowDeath, Log, TEXT("Resurrection request from: %s"), *RequesterName);
    // TODO: Show accept/decline resurrection UI
}

void UWowDeathManager::OnPlayerTeleport(uint32 MapId, float X, float Y, float Z)
{
    // This happens when releasing spirit - teleported to graveyard
    if (bIsPlayerDead)
    {
        UE_LOG(LogWowDeath, Log, TEXT("Player teleported to graveyard: Map %u at (%.1f, %.1f, %.1f)"), MapId, X, Y, Z);
        // Keep death screen visible until player is resurrected
    }
}