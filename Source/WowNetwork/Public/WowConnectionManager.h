#pragma once
#include "CoreMinimal.h"
#include "WowSessionState.h"
#include "WowPacketHandler.h"
#include "WowConnectionManager.generated.h"

class FWowAuthSocket;
class FWowWorldSocket;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSessionStateChanged, EWowSessionState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRealmList, const TArray<FWowRealmInfo>&, Realms);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterList, const TArray<FWowCharacterInfo>&, Characters);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWowError, const FString&, Msg);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharCreateResult, uint8, ResultCode);

enum class EWowActionInvocationKind : uint8
{
    None,
    SpellCast,
    AutoAttack,
    ItemUse,
    Macro,
};

struct FWowActionInvocation
{
    EWowActionInvocationKind Kind = EWowActionInvocationKind::None;
    uint32 ActionId = 0;
    int64 TargetGuid = 0;

    bool IsValid() const
    {
        return Kind != EWowActionInvocationKind::None && ActionId != 0;
    }
};

UCLASS(BlueprintType)
class WOWNETWORK_API UWowConnectionManager : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) void Login(const FString& User, const FString& Pass, const FString& Server, int32 Port = 3724);
    UFUNCTION(BlueprintCallable) void SelectRealm(int32 Index);
    UFUNCTION(BlueprintCallable) void RequestCharacterList();
    UFUNCTION(BlueprintCallable) void EnterWorld(int64 Guid);
    UFUNCTION(BlueprintCallable) void CreateCharacter(const FString& Name, uint8 Race, uint8 Class, uint8 Gender, uint8 Skin = 0, uint8 Face = 0, uint8 HairStyle = 0, uint8 HairColor = 0, uint8 FacialHair = 0);
    UFUNCTION(BlueprintCallable) void DeleteCharacter(int64 Guid);
    UFUNCTION(BlueprintCallable) void Disconnect();
    UFUNCTION(BlueprintCallable) EWowSessionState GetState() const { return State; }

    /** Send a movement packet to the server */
    UFUNCTION(BlueprintCallable) void SendMovement(int32 Opcode, const FVector& Position, float Orientation, int32 MoveFlags);

    /** Send a chat message */
    UFUNCTION(BlueprintCallable) void SendChatMessage(const FString& Message, int32 Type = 1 /*SAY*/, const FString& Target = TEXT(""), int32 Language = 7 /*Common*/);

    /** Send keep-alive heartbeat */
    UFUNCTION(BlueprintCallable) void SendKeepAlive();

    /** Set target selection */
    UFUNCTION(BlueprintCallable) void SendSetSelection(int64 TargetGuid);

    /** Cast a spell on the current target (or self if no target) */
    UFUNCTION(BlueprintCallable) void SendCastSpell(int32 SpellId, int64 TargetGuid = 0);

    /** Assign or clear an action bar slot */
    void SendSetActionButton(int32 SlotIndex, uint32 ActionId, uint8 ActionType = 0);
    void SendClearActionButton(int32 SlotIndex);

    /** Cursor payload helpers for spell/action pickup flows */
    void PickupSpellCursor(int32 SpellId, const FString& BookType = TEXT("spell"));
    void PickupActionCursor(int32 SlotIndex);
    void ClearCursorPayload();
    bool HasCursorPayload() const;
    bool HasCursorSpellPayload() const;
    bool GetCursorInfo(FString& OutType, int32& OutId, FString& OutDetail) const;
    bool PlaceCursorIntoActionSlot(int32 SlotIndex);

    /** Resolve the action assigned to a slot into a concrete gameplay invocation. */
    static FWowActionInvocation ResolveActionInvocation(uint32 PackedAction, int64 TargetGuid);

    /** Use the action assigned to a slot, matching Blizzard action-button behavior. */
    bool UseActionSlot(int32 SlotIndex);

    /** Start melee attack on target */
    UFUNCTION(BlueprintCallable) void SendAttackSwing(int64 TargetGuid);

    /** Stop melee attack */
    UFUNCTION(BlueprintCallable) void SendAttackStop();

    /** Send loot request (right-click corpse to open loot window) */
    UFUNCTION(BlueprintCallable) void SendLoot(int64 CorpseGuid);

    /** Send loot release (close loot window) */
    UFUNCTION(BlueprintCallable) void SendLootRelease(int64 CorpseGuid);

    /** Send loot item command */
    UFUNCTION(BlueprintCallable) void SendLootItem(int32 LootSlot);

    /** Send buy item from vendor command */
    UFUNCTION(BlueprintCallable) void SendBuyItem(int64 VendorGuid, int32 ItemId, int32 Count = 1);

    /** Send sell item to vendor command */
    UFUNCTION(BlueprintCallable) void SendSellItem(int64 VendorGuid, int64 ItemGuid, uint8 Count = 1);

    /** Start a player trade with the target player */
    UFUNCTION(BlueprintCallable) void SendInitiateTrade(int64 TargetGuid);

    /** Accept an incoming trade request and open the trade window */
    UFUNCTION(BlueprintCallable) void SendBeginTrade();

    /** Accept the current trade contents */
    UFUNCTION(BlueprintCallable) void SendAcceptTrade();

    /** Revoke local trade acceptance */
    UFUNCTION(BlueprintCallable) void SendUnacceptTrade();

    /** Cancel the current trade */
    UFUNCTION(BlueprintCallable) void SendCancelTrade();

    /** Set the amount of gold offered in the current trade */
    UFUNCTION(BlueprintCallable) void SendSetTradeGold(int32 CopperAmount);

    /** Accept the currently pending duel request */
    void SendAcceptDuel(int64 ArbiterGuid);

    /** Decline a pending duel request or forfeit an active duel */
    void SendCancelDuel(int64 ArbiterGuid);

    /** Request the current pet action bar state from the server */
    void SendRequestPetInfo();

    /** Trigger the pet action currently assigned to a pet action bar slot */
    void SendPetActionBarSlot(int32 SlotIndex, int64 TargetGuid = 0);

    /** Toggle pet spell autocast for the action currently assigned to a slot */
    void SendPetSpellAutocast(int32 SlotIndex, bool bEnabled);

    /** Request the current raid target icon assignments */
    void SendRequestRaidTargetIcons();

    /** Assign a raid target icon to the specified guid (or clear by passing 0) */
    void SendSetRaidTargetIcon(int32 IconIndex, int64 InTargetGuid);

    /** Start a ready check if the local player can issue one */
    void SendStartReadyCheck();

    /** Send a ready-check response */
    void SendReadyCheckConfirm(bool bReady);

    /** Finish the active ready check */
    void SendFinishReadyCheck();

    /** Send gossip/questgiver hello to initiate NPC interaction */
    UFUNCTION(BlueprintCallable) void SendGossipHello(int64 NpcGuid);

    /** Use an interactable gameobject such as a mailbox */
    UFUNCTION(BlueprintCallable) void SendGameObjectUse(int64 GameObjectGuid);

    /** Send banker activate to open the bank window */
    UFUNCTION(BlueprintCallable) void SendBankerActivate(int64 BankerGuid);

    /** Request mailbox contents from a mailbox gameobject or mailbox NPC */
    UFUNCTION(BlueprintCallable) void SendGetMailList(int64 MailboxGuid);

    /** Send quest accept command */
    UFUNCTION(BlueprintCallable) void SendQuestAccept(int64 QuestGiverGuid, int32 QuestId);

    /** Send quest complete/choose reward command */
    UFUNCTION(BlueprintCallable) void SendQuestChooseReward(int64 QuestGiverGuid, int32 QuestId, int32 RewardChoice = 0);

    /** Get current target GUID */
    UFUNCTION(BlueprintCallable) int64 GetTargetGuid() const { return TargetGuid; }

    /** Get the character name of the local player (from cached character list) */
    UFUNCTION(BlueprintCallable) FString GetCharacterName() const
    {
        uint64 Guid = PacketHandler.EntityManager.LocalPlayerGuid;
        for (const FWowCharacterInfo& C : CachedCharacters)
        {
            if (static_cast<uint64>(C.Guid) == Guid) return C.Name;
        }
        return TEXT("");
    }

    /** Send a raw packet (for advanced use) */
    void SendRawPacket(uint32 Opcode, const TArray<uint8>& Data = {});

    /** Send name query for a player */
    UFUNCTION(BlueprintCallable) void SendNameQuery(int64 Guid);

    /** Send creature query for an NPC */
    UFUNCTION(BlueprintCallable) void SendCreatureQuery(int32 Entry, int64 Guid);

    /** Send text emote command */
    UFUNCTION(BlueprintCallable) void SendTextEmote(int32 EmoteTextId, int64 TargetGuid = 0);

    /** Send learn talent command */
    UFUNCTION(BlueprintCallable) void SendLearnTalent(int32 TalentId, int32 RequestedRank);

    /** Get the cached character list (valid after WorldHaveCharList state) */
    UFUNCTION(BlueprintCallable) TArray<FWowCharacterInfo> GetCachedCharacters() const { return CachedCharacters; }

    /** Get the cached realm list (valid after AuthHaveRealmList state) */
    UFUNCTION(BlueprintCallable) TArray<FWowRealmInfo> GetCachedRealms() const { return CachedRealms; }

    /** Packet handler — dispatches SMSG opcodes and tracks entities */
    FWowPacketHandler PacketHandler;

    UPROPERTY(BlueprintAssignable) FOnSessionStateChanged OnStateChanged;
    UPROPERTY(BlueprintAssignable) FOnRealmList OnRealmList;
    UPROPERTY(BlueprintAssignable) FOnCharacterList OnCharacterList;
    UPROPERTY(BlueprintAssignable) FOnWowError OnError;
    UPROPERTY(BlueprintAssignable) FOnCharCreateResult OnCharCreateResult;
private:
    enum class ECursorPayloadType : uint8
    {
        None,
        Spell,
        Action,
    };

    struct FCursorPayloadState
    {
        ECursorPayloadType Type = ECursorPayloadType::None;
        int32 PrimaryId = 0;
        FString Detail;
        uint8 ActionType = 0;
        int32 SourceActionSlot = -1;
    };

    EWowSessionState State = EWowSessionState::Disconnected;
    void SetState(EWowSessionState S);
    void BroadcastActionButtonsChanged();
    void EnsureActionButtonCapacity(int32 SlotIndex);

    void OnAuthResultReceived(bool bSuccess);
    void OnRealmListReceived(const TArray<FWowRealmInfo>& Realms);
    void OnWorldAuthResult(bool bSuccess);
    void OnWorldCharacterList(const TArray<FWowCharacterInfo>& Characters);

    TSharedPtr<FWowAuthSocket> AuthSocket;
    TSharedPtr<FWowWorldSocket> WorldSocket;
    FString CachedAccountName;
    TArray<uint8> SessionKey;
    TArray<FWowRealmInfo> CachedRealms;
    TArray<FWowCharacterInfo> CachedCharacters;
    int64 TargetGuid = 0;
    FCursorPayloadState CursorPayload;
};
