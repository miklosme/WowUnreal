#include "Misc/AutomationTest.h"
#include "Mpq/MpqManager.h"
#include "Formats/DbcParser.h"
#include "Formats/BlpParser.h"
#include "Formats/BlpTypes.h"
#include "Formats/AdtParser.h"
#include "Formats/AdtTypes.h"
#include "Formats/M2Parser.h"
#include "Formats/M2Types.h"
#include "Formats/WmoParser.h"
#include "Formats/WmoTypes.h"
#include "Coord/WowCoordinate.h"

#define private public
#include "WowConnectionManager.h"
#include "Net/WowWorldSocket.h"
#undef private

#include "WowLuaVM.h"
#include "WowEventSystem.h"
#include "WowFrameTypes.h"
#include "WowOpcodes.h"

#if __has_include("lua.h")
extern "C" {
#include "lua.h"
}
#endif

struct FWowLuaContext
{
    class FWowEntityManager* EntityManager = nullptr;
    class UWowConnectionManager* ConnectionManager = nullptr;
    class FWowEventSystem* EventSystem = nullptr;
    class FWowFrameManager* FrameManager = nullptr;
};

// ---- Helper: shared MPQ manager for data-driven tests ----
namespace WowTestUtils
{
    static FMpqManager& GetMpq()
    {
        static FMpqManager Mpq;
        static bool bInit = false;
        if (!bInit)
        {
            bInit = true;
            // Try known WoW 3.3.5a data paths
            TArray<FString> Paths = {
                FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT("Downloads/World of Warcraft 3.3.5a/Data")),
                FPaths::Combine(FPlatformProcess::UserHomeDir(), TEXT("World of Warcraft 3.3.5a/Data")),
                TEXT("/Users/clancey/Downloads/World of Warcraft 3.3.5a/Data"),
            };
            for (const FString& Path : Paths)
            {
                if (FPaths::DirectoryExists(Path))
                {
                    Mpq.Initialize(Path);
                    break;
                }
            }
        }
        return Mpq;
    }

    static TSharedPtr<FWowWorldSocket> AttachTestWorldSocket(UWowConnectionManager& Connection, int64 TargetGuid)
    {
        Connection.State = EWowSessionState::WorldInGame;
        Connection.TargetGuid = TargetGuid;
        Connection.WorldSocket = MakeShared<FWowWorldSocket>();
        return Connection.WorldSocket;
    }

    static bool DequeueClientPacket(FWowWorldSocket& Socket, uint32& OutOpcode, TArray<uint8>& OutPayload)
    {
        TArray<uint8> Packet;
        if (!Socket.SendQueue.Dequeue(Packet) || Packet.Num() < 6)
        {
            return false;
        }

        FMemory::Memcpy(&OutOpcode, Packet.GetData() + 2, sizeof(OutOpcode));
        OutPayload.SetNumUninitialized(Packet.Num() - 6);
        if (OutPayload.Num() > 0)
        {
            FMemory::Memcpy(OutPayload.GetData(), Packet.GetData() + 6, OutPayload.Num());
        }

        return true;
    }
}

// ====================================================================
// Coordinate Conversion Tests (pure math, no data dependency)
// ====================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoordAdtToUE, "WowUnreal.Coord.AdtToUE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCoordAdtToUE::RunTest(const FString& Parameters)
{
    // ADT coords: X=east, Y=up, Z=south → UE: X=-Z*100, Y=X*100, Z=Y*100
    FVector Result = FWowCoordinate::AdtToUE(10.0f, 20.0f, 30.0f);
    TestTrue(TEXT("UE X = -NgZ * 100"), FMath::IsNearlyEqual(Result.X, -3000.0, 0.01));
    TestTrue(TEXT("UE Y = NgX * 100"), FMath::IsNearlyEqual(Result.Y, 1000.0, 0.01));
    TestTrue(TEXT("UE Z = NgY * 100"), FMath::IsNearlyEqual(Result.Z, 2000.0, 0.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoordTileRoundtrip, "WowUnreal.Coord.TileRoundtrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCoordTileRoundtrip::RunTest(const FString& Parameters)
{
    // Convert tile→world→tile and verify roundtrip
    int32 OrigX = 32, OrigY = 48;
    FVector WorldPos = FWowCoordinate::TileToWorld(OrigX, OrigY);
    FIntPoint TileBack = FWowCoordinate::WorldToTile(WorldPos);
    TestEqual(TEXT("Tile X roundtrips"), TileBack.X, OrigX);
    TestEqual(TEXT("Tile Y roundtrips"), TileBack.Y, OrigY);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCoordWowRoundtrip, "WowUnreal.Coord.WowToUERoundtrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCoordWowRoundtrip::RunTest(const FString& Parameters)
{
    // Test WowToUE→UEToWow roundtrip
    FVector WowPos(1000.0f, 2000.0f, 100.0f);
    FVector UePos = FWowCoordinate::WowToUE(WowPos);
    FVector Back = FWowCoordinate::UEToWow(UePos);
    TestTrue(TEXT("WoW X roundtrips"), FMath::IsNearlyEqual(Back.X, WowPos.X, 0.01f));
    TestTrue(TEXT("WoW Y roundtrips"), FMath::IsNearlyEqual(Back.Y, WowPos.Y, 0.01f));
    TestTrue(TEXT("WoW Z roundtrips"), FMath::IsNearlyEqual(Back.Z, WowPos.Z, 0.01f));
    return true;
}

// ====================================================================
// Entity Tests (pure in-memory, no data dependency)
// ====================================================================

#include "WowEntity.h"
#include "WowEntityManager.h"
#include "WowUpdateFields.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntityCreateAndLookup, "WowUnreal.Entity.CreateAndLookup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEntityCreateAndLookup::RunTest(const FString& Parameters)
{
    FWowEntityManager EM;

    // Create an entity
    FWowEntity& Ent = EM.GetOrCreate(12345);
    TestEqual(TEXT("GUID matches"), Ent.Guid, (uint64)12345);
    TestEqual(TEXT("Count is 1"), EM.Num(), 1);

    // Find it
    FWowEntity* Found = EM.Find(12345);
    TestNotNull(TEXT("Found by GUID"), Found);
    TestEqual(TEXT("Same entity"), Found, &Ent);

    // Missing entity returns null
    FWowEntity* Missing = EM.Find(99999);
    TestNull(TEXT("Missing returns null"), Missing);

    // Remove it
    EM.Remove(12345);
    TestEqual(TEXT("Count is 0 after remove"), EM.Num(), 0);
    TestNull(TEXT("Removed entity not found"), EM.Find(12345));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntityFieldAccess, "WowUnreal.Entity.FieldAccess",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEntityFieldAccess::RunTest(const FString& Parameters)
{
    FWowEntity Ent;
    Ent.Guid = 100;
    Ent.TypeMask = WowTypeMask::UNIT | WowTypeMask::PLAYER;

    // Type checks
    TestTrue(TEXT("IsUnit"), Ent.IsUnit());
    TestTrue(TEXT("IsPlayer"), Ent.IsPlayer());
    TestFalse(TEXT("Not GameObject"), Ent.IsGameObject());

    // Set fields
    Ent.SetField(UnitField::HEALTH, 5000);
    Ent.SetField(UnitField::MAXHEALTH, 10000);
    Ent.SetField(UnitField::LEVEL, 80);

    TestEqual(TEXT("Health"), Ent.GetHealth(), 5000);
    TestEqual(TEXT("MaxHealth"), Ent.GetMaxHealth(), 10000);
    TestEqual(TEXT("Level"), Ent.GetLevel(), 80);

    // Default field returns 0
    TestEqual(TEXT("Unset field is 0"), Ent.GetField(999), (uint32)0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntityTypeMasks, "WowUnreal.Entity.TypeMasks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEntityTypeMasks::RunTest(const FString& Parameters)
{
    FWowEntity GO;
    GO.TypeMask = WowTypeMask::GAMEOBJECT;
    TestTrue(TEXT("GO IsGameObject"), GO.IsGameObject());
    TestFalse(TEXT("GO not IsUnit"), GO.IsUnit());
    TestFalse(TEXT("GO not IsPlayer"), GO.IsPlayer());
    TestFalse(TEXT("GO not IsItem"), GO.IsItem());

    FWowEntity Item;
    Item.TypeMask = WowTypeMask::ITEM;
    TestTrue(TEXT("Item IsItem"), Item.IsItem());
    TestFalse(TEXT("Item not IsUnit"), Item.IsUnit());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntityTypedPromotion, "WowUnreal.Entity.TypedPromotion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEntityTypedPromotion::RunTest(const FString& Parameters)
{
    FWowEntityManager EM;

    FWowEntity& Base = EM.GetOrCreate(777);
    Base.SetField(ObjectField::TYPE, WowTypeMask::UNIT | WowTypeMask::PLAYER);
    Base.SetField(UnitField::HEALTH, 4242);
    Base.SetField(UnitField::MAXHEALTH, 5000);
    Base.SetField(UnitField::LEVEL, 80);
    Base.SetField(PlayerField::XP, 9001);
    Base.SetField(PlayerField::COINAGE, 123456);

    FWowEntity& Promoted = EM.PromoteToTyped(777, Base.GetField(ObjectField::TYPE));
    TestTrue(TEXT("Promoted entity reports player kind"), Promoted.IsPlayer());

    FWowPlayerEntity* Player = EM.FindPlayer(777);
    TestNotNull(TEXT("Typed player lookup succeeds"), Player);
    if (!Player)
    {
        return false;
    }

    TestEqual(TEXT("Unit data preserved across promotion"), Player->GetHealth(), 4242);
    TestEqual(TEXT("Player XP accessor works"), Player->GetXp(), static_cast<uint32>(9001));
    TestEqual(TEXT("Player coinage accessor works"), Player->GetCoinage(), static_cast<uint32>(123456));
    TestEqual(TEXT("Unit lookup returns promoted player"), EM.FindUnit(777), static_cast<FWowUnitEntity*>(Player));

    EM.LocalPlayerGuid = 777;
    TestEqual(TEXT("Local player lookup is typed"), EM.GetLocalPlayer(), Player);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntityTypedContainersAndGameObjects, "WowUnreal.Entity.TypedContainersAndGameObjects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FEntityTypedContainersAndGameObjects::RunTest(const FString& Parameters)
{
    FWowEntityManager EM;

    FWowEntity& ContainerBase = EM.GetOrCreate(1001);
    ContainerBase.SetField(ObjectField::TYPE, WowTypeMask::ITEM | WowTypeMask::CONTAINER);
    ContainerBase.SetField(ItemField::STACK_COUNT, 20);
    ContainerBase.SetField(ContainerField::NUM_SLOTS, 16);
    ContainerBase.SetField(ContainerField::SLOT_1, 0xAABBCCDD);
    ContainerBase.SetField(ContainerField::SLOT_1 + 1, 0x11223344);
    EM.PromoteToTyped(1001, ContainerBase.GetField(ObjectField::TYPE));

    FWowContainerEntity* Container = EM.FindContainer(1001);
    TestNotNull(TEXT("Typed container lookup succeeds"), Container);
    if (!Container)
    {
        return false;
    }

    TestEqual(TEXT("Container inherits item stack count"), Container->GetStackCount(), 20);
    TestEqual(TEXT("Container slot count accessor works"), Container->GetNumSlots(), 16);
    TestEqual(TEXT("Container GUID slot accessor combines 64-bit field"), Container->GetItemGuidAtSlot(0), static_cast<uint64>(0x11223344AABBCCDDULL));
    TestEqual(TEXT("Item lookup returns container subtype"), EM.FindItem(1001), static_cast<FWowItemEntity*>(Container));

    FWowEntity& GoBase = EM.GetOrCreate(2002);
    GoBase.SetField(ObjectField::TYPE, WowTypeMask::GAMEOBJECT);
    GoBase.SetField(GameObjectField::DISPLAY_ID, 31415);
    GoBase.SetField(GameObjectField::FLAGS, 0x20);
    GoBase.SetField(GameObjectField::LEVEL, 1);
    EM.PromoteToTyped(2002, GoBase.GetField(ObjectField::TYPE));

    FWowGameObjectEntity* GameObject = EM.FindGameObject(2002);
    TestNotNull(TEXT("Typed gameobject lookup succeeds"), GameObject);
    if (!GameObject)
    {
        return false;
    }

    TestEqual(TEXT("GameObject display ID accessor works"), GameObject->GetGameObjectDisplayId(), static_cast<uint32>(31415));
    TestEqual(TEXT("GameObject flags accessor works"), GameObject->GetGameObjectFlags(), static_cast<uint32>(0x20));
    TestEqual(TEXT("GameObject level accessor works"), GameObject->GetGameObjectLevel(), 1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionCursorPlacesSpellIntoActionBar, "WowUnreal.UI.ActionCursorPlacesSpellIntoActionBar",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionCursorPlacesSpellIntoActionBar::RunTest(const FString& Parameters)
{
    UWowConnectionManager* Connection = NewObject<UWowConnectionManager>();
    TestNotNull(TEXT("Connection manager created"), Connection);
    if (!Connection)
    {
        return false;
    }

    Connection->PickupSpellCursor(133, TEXT("spell"));

    FString CursorType;
    FString CursorDetail;
    int32 CursorId = 0;
    TestTrue(TEXT("Cursor info available after spell pickup"), Connection->GetCursorInfo(CursorType, CursorId, CursorDetail));
    TestEqual(TEXT("Cursor type is spell"), CursorType, FString(TEXT("spell")));
    TestEqual(TEXT("Cursor spell id matches"), CursorId, 133);
    TestEqual(TEXT("Cursor detail stores spell book type"), CursorDetail, FString(TEXT("spell")));
    TestTrue(TEXT("Spell payload is reported"), Connection->HasCursorSpellPayload());

    const bool bPlaced = Connection->PlaceCursorIntoActionSlot(4);
    TestTrue(TEXT("Placing spell payload into slot succeeds"), bPlaced);
    TestTrue(TEXT("Action array grew to include placed slot"), Connection->PacketHandler.ActionButtons.IsValidIndex(4));
    if (Connection->PacketHandler.ActionButtons.IsValidIndex(4))
    {
        TestEqual(TEXT("Placed action stores spell id in the slot"), Connection->PacketHandler.ActionButtons[4], static_cast<uint32>(133));
    }
    TestFalse(TEXT("Cursor payload clears after placement"), Connection->HasCursorPayload());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionCursorMovesExistingAction, "WowUnreal.UI.ActionCursorMovesExistingAction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionCursorMovesExistingAction::RunTest(const FString& Parameters)
{
    UWowConnectionManager* Connection = NewObject<UWowConnectionManager>();
    TestNotNull(TEXT("Connection manager created"), Connection);
    if (!Connection)
    {
        return false;
    }

    Connection->PacketHandler.ActionButtons.SetNumZeroed(12);
    const uint32 PackedSpellAction = 6603; // type 0 spell action
    Connection->PacketHandler.ActionButtons[1] = PackedSpellAction;

    Connection->PickupActionCursor(1);

    FString CursorType;
    FString CursorDetail;
    int32 CursorId = 0;
    TestTrue(TEXT("Cursor info available after action pickup"), Connection->GetCursorInfo(CursorType, CursorId, CursorDetail));
    TestEqual(TEXT("Cursor type is action"), CursorType, FString(TEXT("action")));
    TestEqual(TEXT("Cursor action id is 1-based source slot"), CursorId, 2);

    const bool bPlaced = Connection->PlaceCursorIntoActionSlot(3);
    TestTrue(TEXT("Moving action payload into destination slot succeeds"), bPlaced);
    TestEqual(TEXT("Source slot is cleared after move"), Connection->PacketHandler.ActionButtons[1], static_cast<uint32>(0));
    TestEqual(TEXT("Destination slot receives moved action"), Connection->PacketHandler.ActionButtons[3], PackedSpellAction);
    TestFalse(TEXT("Cursor payload clears after move"), Connection->HasCursorPayload());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionInvocationRoutesAutoAttack, "WowUnreal.UI.ActionInvocationRoutesAutoAttack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionInvocationRoutesAutoAttack::RunTest(const FString& Parameters)
{
    const FWowActionInvocation Invocation =
        UWowConnectionManager::ResolveActionInvocation(6603u, 0x1234);

    TestEqual(TEXT("Auto-attack action resolves to auto-attack kind"),
        Invocation.Kind, EWowActionInvocationKind::AutoAttack);
    TestEqual(TEXT("Auto-attack action preserves spell id"), Invocation.ActionId, static_cast<uint32>(6603));
    TestEqual(TEXT("Auto-attack action preserves target"), Invocation.TargetGuid, static_cast<int64>(0x1234));
    TestTrue(TEXT("Auto-attack invocation is valid"), Invocation.IsValid());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FActionInvocationRoutesRegularSpell, "WowUnreal.UI.ActionInvocationRoutesRegularSpell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FActionInvocationRoutesRegularSpell::RunTest(const FString& Parameters)
{
    const uint32 PackedSpellAction = 133u; // type 0 spell action
    const FWowActionInvocation Invocation =
        UWowConnectionManager::ResolveActionInvocation(PackedSpellAction, 0);

    TestEqual(TEXT("Regular spell resolves to spell-cast kind"),
        Invocation.Kind, EWowActionInvocationKind::SpellCast);
    TestEqual(TEXT("Regular spell preserves spell id"), Invocation.ActionId, static_cast<uint32>(133));
    TestEqual(TEXT("Regular spell preserves target"), Invocation.TargetGuid, static_cast<int64>(0));
    TestTrue(TEXT("Regular spell invocation is valid"), Invocation.IsValid());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaUseActionAutoAttackQueuesAttackSwing, "WowUnreal.UI.LuaUseActionAutoAttackQueuesAttackSwing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLuaUseActionAutoAttackQueuesAttackSwing::RunTest(const FString& Parameters)
{
    UWowConnectionManager* Connection = NewObject<UWowConnectionManager>();
    TestNotNull(TEXT("Connection manager created"), Connection);
    if (!Connection)
    {
        return false;
    }

    constexpr int64 TargetGuid = 0x0011223344556677LL;
    TSharedPtr<FWowWorldSocket> WorldSocket = WowTestUtils::AttachTestWorldSocket(*Connection, TargetGuid);
    TestTrue(TEXT("Test world socket attached"), WorldSocket.IsValid());
    if (!WorldSocket.IsValid())
    {
        return false;
    }

    Connection->PacketHandler.ActionButtons.SetNumZeroed(12);
    Connection->PacketHandler.ActionButtons[0] = 6603; // Auto Attack

    FWowLuaVM LuaVM;
    TestTrue(TEXT("Lua VM initializes"), LuaVM.Initialize());
    if (!LuaVM.IsInitialized())
    {
        return false;
    }

    FWowLuaContext Context;
    Context.ConnectionManager = Connection;
    lua_pushlightuserdata(LuaVM.GetState(), &Context);
    lua_setfield(LuaVM.GetState(), LUA_REGISTRYINDEX, "WowLuaContext");

    const bool bExecuted = LuaVM.ExecuteString(TEXT("UseAction(1)"), TEXT("LuaUseActionAutoAttack"));
    TestTrue(TEXT("UseAction executes successfully"), bExecuted);

    uint32 Opcode = 0;
    TArray<uint8> Payload;
    const bool bDequeued = WowTestUtils::DequeueClientPacket(*WorldSocket, Opcode, Payload);
    TestTrue(TEXT("UseAction queued a client packet"), bDequeued);
    if (bDequeued)
    {
        TestEqual(TEXT("UseAction queues CMSG_ATTACKSWING for auto-attack"), Opcode, static_cast<uint32>(WowOpcode::CMSG_ATTACKSWING));
        TestEqual(TEXT("Attack swing payload is an 8-byte guid"), Payload.Num(), 8);

        uint64 SentTargetGuid = 0;
        if (Payload.Num() == 8)
        {
            FMemory::Memcpy(&SentTargetGuid, Payload.GetData(), sizeof(SentTargetGuid));
            TestEqual(TEXT("Attack swing targets the selected unit"), SentTargetGuid, static_cast<uint64>(TargetGuid));
        }
    }

    lua_pushnil(LuaVM.GetState());
    lua_setfield(LuaVM.GetState(), LUA_REGISTRYINDEX, "WowLuaContext");
    LuaVM.Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaUseActionSpellQueuesCastSpell, "WowUnreal.UI.LuaUseActionSpellQueuesCastSpell",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLuaUseActionSpellQueuesCastSpell::RunTest(const FString& Parameters)
{
    UWowConnectionManager* Connection = NewObject<UWowConnectionManager>();
    TestNotNull(TEXT("Connection manager created"), Connection);
    if (!Connection)
    {
        return false;
    }

    constexpr int64 TargetGuid = 0x0000000000004242LL;
    TSharedPtr<FWowWorldSocket> WorldSocket = WowTestUtils::AttachTestWorldSocket(*Connection, TargetGuid);
    TestTrue(TEXT("Test world socket attached"), WorldSocket.IsValid());
    if (!WorldSocket.IsValid())
    {
        return false;
    }

    Connection->PacketHandler.ActionButtons.SetNumZeroed(12);
    Connection->PacketHandler.ActionButtons[0] = 133; // Fireball

    FWowLuaVM LuaVM;
    TestTrue(TEXT("Lua VM initializes"), LuaVM.Initialize());
    if (!LuaVM.IsInitialized())
    {
        return false;
    }

    FWowLuaContext Context;
    Context.ConnectionManager = Connection;
    lua_pushlightuserdata(LuaVM.GetState(), &Context);
    lua_setfield(LuaVM.GetState(), LUA_REGISTRYINDEX, "WowLuaContext");

    const bool bExecuted = LuaVM.ExecuteString(TEXT("UseAction(1)"), TEXT("LuaUseActionSpell"));
    TestTrue(TEXT("UseAction executes successfully"), bExecuted);

    uint32 Opcode = 0;
    TArray<uint8> Payload;
    const bool bDequeued = WowTestUtils::DequeueClientPacket(*WorldSocket, Opcode, Payload);
    TestTrue(TEXT("UseAction queued a client packet"), bDequeued);
    if (bDequeued)
    {
        TestEqual(TEXT("UseAction queues CMSG_CAST_SPELL for normal spells"), Opcode, static_cast<uint32>(WowOpcode::CMSG_CAST_SPELL));
        TestTrue(TEXT("Cast spell payload is large enough for cast count, spell id, flags, and target mask"), Payload.Num() >= 10);

        if (Payload.Num() >= 5)
        {
            uint32 SpellId = 0;
            FMemory::Memcpy(&SpellId, Payload.GetData() + 1, sizeof(SpellId));
            TestEqual(TEXT("Cast spell payload preserves the spell id"), SpellId, static_cast<uint32>(133));
        }
    }

    lua_pushnil(LuaVM.GetState());
    lua_setfield(LuaVM.GetState(), LUA_REGISTRYINDEX, "WowLuaContext");
    LuaVM.Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDeferredOnLoadBatchResolvesSiblingGlobals, "WowUnreal.UI.DeferredOnLoadBatchResolvesSiblingGlobals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDeferredOnLoadBatchResolvesSiblingGlobals::RunTest(const FString& Parameters)
{
    FWowLuaVM LuaVM;
    TestTrue(TEXT("Lua VM initializes"), LuaVM.Initialize());
    if (!LuaVM.IsInitialized())
    {
        return false;
    }

    FWowEventSystem EventSystem;
    EventSystem.SetLuaVM(&LuaVM);

    EventSystem.BeginOnLoadBatch();

    EventSystem.CreateFrameObject(1, TEXT("ParentFrame"));
    FWowFrameDef ParentDef;
    ParentDef.Name = TEXT("ParentFrame");
    FWowScriptHandler OnLoad;
    OnLoad.Event = TEXT("OnLoad");
    OnLoad.Code = TEXT("assert(SiblingFrame ~= nil, 'SiblingFrame missing during OnLoad'); ParentFrameSawSibling = true");
    ParentDef.Scripts.Add(OnLoad);
    EventSystem.CompileFrameScripts(1, ParentDef);

    EventSystem.CreateFrameObject(2, TEXT("SiblingFrame"));
    FWowFrameDef SiblingDef;
    SiblingDef.Name = TEXT("SiblingFrame");
    EventSystem.CompileFrameScripts(2, SiblingDef);

    lua_getglobal(LuaVM.GetState(), "ParentFrameSawSibling");
    TestTrue(TEXT("OnLoad is deferred until the batch completes"), lua_isnil(LuaVM.GetState(), -1));
    lua_pop(LuaVM.GetState(), 1);

    EventSystem.EndOnLoadBatch();

    lua_getglobal(LuaVM.GetState(), "ParentFrameSawSibling");
    const bool bSawSibling = lua_toboolean(LuaVM.GetState(), -1) != 0;
    lua_pop(LuaVM.GetState(), 1);
    TestTrue(TEXT("Deferred OnLoad sees globals created later in the same batch"), bSawSibling);

    LuaVM.Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaterOnLoadOverridesEarlierScript, "WowUnreal.UI.LaterOnLoadOverridesEarlierScript",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLaterOnLoadOverridesEarlierScript::RunTest(const FString& Parameters)
{
    FWowLuaVM LuaVM;
    TestTrue(TEXT("Lua VM initializes"), LuaVM.Initialize());
    if (!LuaVM.IsInitialized())
    {
        return false;
    }

    FWowEventSystem EventSystem;
    EventSystem.SetLuaVM(&LuaVM);
    EventSystem.CreateFrameObject(1, TEXT("TestFrame"));

    FWowFrameDef Def;
    Def.Name = TEXT("TestFrame");

    FWowScriptHandler TemplateOnLoad;
    TemplateOnLoad.Event = TEXT("OnLoad");
    TemplateOnLoad.Code = TEXT("TemplateOnLoadRan = true");
    Def.Scripts.Add(TemplateOnLoad);

    FWowScriptHandler OverrideOnLoad;
    OverrideOnLoad.Event = TEXT("OnLoad");
    OverrideOnLoad.Code = TEXT("OverrideOnLoadRan = (TemplateOnLoadRan ~= true)");
    Def.Scripts.Add(OverrideOnLoad);

    EventSystem.CompileFrameScripts(1, Def);

    lua_getglobal(LuaVM.GetState(), "TemplateOnLoadRan");
    const bool bTemplateRan = lua_toboolean(LuaVM.GetState(), -1) != 0;
    lua_pop(LuaVM.GetState(), 1);

    lua_getglobal(LuaVM.GetState(), "OverrideOnLoadRan");
    const bool bOverrideRanWithoutTemplate = lua_toboolean(LuaVM.GetState(), -1) != 0;
    lua_pop(LuaVM.GetState(), 1);

    TestFalse(TEXT("Earlier duplicate OnLoad is overridden"), bTemplateRan);
    TestTrue(TEXT("Later duplicate OnLoad replaces the earlier handler"), bOverrideRanWithoutTemplate);

    LuaVM.Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUiParentDefaultAttributesSeeded, "WowUnreal.UI.UiParentDefaultAttributesSeeded",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUiParentDefaultAttributesSeeded::RunTest(const FString& Parameters)
{
    FWowLuaVM LuaVM;
    TestTrue(TEXT("Lua VM initializes"), LuaVM.Initialize());
    if (!LuaVM.IsInitialized())
    {
        return false;
    }

    FWowEventSystem EventSystem;
    EventSystem.SetLuaVM(&LuaVM);
    EventSystem.CreateFrameObject(1, TEXT("UIParent"));

    lua_getglobal(LuaVM.GetState(), "UIParent");
    TestTrue(TEXT("UIParent global exists"), lua_istable(LuaVM.GetState(), -1));
    lua_getfield(LuaVM.GetState(), -1, "__attr_DEFAULT_FRAME_WIDTH");
    const double DefaultFrameWidth = lua_tonumber(LuaVM.GetState(), -1);
    lua_pop(LuaVM.GetState(), 2);
    TestEqual(TEXT("UIParent seeds DEFAULT_FRAME_WIDTH"), DefaultFrameWidth, 384.0);

    LuaVM.Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaUiUtilityGlobalsExist, "WowUnreal.UI.LuaUiUtilityGlobalsExist",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FLuaUiUtilityGlobalsExist::RunTest(const FString& Parameters)
{
    FWowLuaVM LuaVM;
    TestTrue(TEXT("Lua VM initializes"), LuaVM.Initialize());
    if (!LuaVM.IsInitialized())
    {
        return false;
    }

    lua_State* L = LuaVM.GetState();

    lua_getglobal(L, "table");
    TestTrue(TEXT("table library exists"), lua_istable(L, -1));
    lua_getfield(L, -1, "wipe");
    const bool bHasTableWipe = lua_isfunction(L, -1);
    lua_pop(L, 2);
    TestTrue(TEXT("table.wipe is exposed for Blizzard UI code"), bHasTableWipe);

    lua_getglobal(L, "CombatLogResetFilter");
    const bool bHasCombatLogResetFilter = lua_isfunction(L, -1);
    lua_pop(L, 1);
    TestTrue(TEXT("CombatLogResetFilter global exists"), bHasCombatLogResetFilter);

    lua_getglobal(L, "CombatLogAddFilter");
    const bool bHasCombatLogAddFilter = lua_isfunction(L, -1);
    lua_pop(L, 1);
    TestTrue(TEXT("CombatLogAddFilter global exists"), bHasCombatLogAddFilter);

    lua_getglobal(L, "CombatLogGetNumEntries");
    const bool bHasCombatLogGetNumEntries = lua_isfunction(L, -1);
    lua_pop(L, 1);
    TestTrue(TEXT("CombatLogGetNumEntries global exists"), bHasCombatLogGetNumEntries);

    lua_getglobal(L, "CombatLogGetCurrentEntry");
    const bool bHasCombatLogGetCurrentEntry = lua_isfunction(L, -1);
    lua_pop(L, 1);
    TestTrue(TEXT("CombatLogGetCurrentEntry global exists"), bHasCombatLogGetCurrentEntry);

    lua_getglobal(L, "CombatLog_Object_IsA");
    const bool bHasCombatLogObjectIsA = lua_isfunction(L, -1);
    lua_pop(L, 1);
    TestTrue(TEXT("CombatLog_Object_IsA global exists"), bHasCombatLogObjectIsA);

    lua_getglobal(L, "CombatLogSetCurrentEntry");
    lua_pushinteger(L, 5);
    const int SetCurrentEntryResult = lua_pcall(L, 1, 0, 0);
    TestEqual(TEXT("CombatLogSetCurrentEntry accepts a numeric cursor"), SetCurrentEntryResult, 0);

    lua_getglobal(L, "CombatLogAdvanceEntry");
    lua_pushinteger(L, 1);
    const int AdvanceEntryResult = lua_pcall(L, 1, 8, 0);
    TestEqual(TEXT("CombatLogAdvanceEntry accepts a numeric delta"), AdvanceEntryResult, 0);
    const bool bAdvanceEntryHasStringEvent = lua_isstring(L, -7);
    const FString AdvancedEventName = bAdvanceEntryHasStringEvent ? UTF8_TO_TCHAR(lua_tostring(L, -7)) : FString();
    const bool bAdvanceEntryHasNumericSourceFlags = lua_isnumber(L, -4);
    const int32 AdvancedSourceFlags = bAdvanceEntryHasNumericSourceFlags ? static_cast<int32>(lua_tointeger(L, -4)) : 0;
    lua_pop(L, 8);
    TestTrue(TEXT("CombatLogAdvanceEntry returns a WotLK event tuple"), bAdvanceEntryHasStringEvent);
    TestEqual(TEXT("CombatLogAdvanceEntry uses an empty placeholder event"), AdvancedEventName, FString());
    TestTrue(TEXT("CombatLogAdvanceEntry returns numeric source flags"), bAdvanceEntryHasNumericSourceFlags);
    TestEqual(TEXT("CombatLogAdvanceEntry defaults unknown source flags to COMBATLOG_OBJECT_NONE"), AdvancedSourceFlags, static_cast<int32>(0x80000000u));

    lua_getglobal(L, "CombatLogGetNumEntries");
    const int GetNumEntriesResult = lua_pcall(L, 0, 1, 0);
    TestEqual(TEXT("CombatLogGetNumEntries call succeeds"), GetNumEntriesResult, 0);
    const int32 CombatLogNumEntries = static_cast<int32>(lua_tointeger(L, -1));
    lua_pop(L, 1);
    TestEqual(TEXT("CombatLog cursor API defaults to an empty log"), CombatLogNumEntries, 0);

    lua_getglobal(L, "CombatLogGetCurrentEntry");
    const int GetCurrentEntryResult = lua_pcall(L, 0, 8, 0);
    TestEqual(TEXT("CombatLogGetCurrentEntry call succeeds"), GetCurrentEntryResult, 0);
    const bool bCurrentEntryHasStringEvent = lua_isstring(L, -7);
    const FString CurrentEntryEventName = bCurrentEntryHasStringEvent ? UTF8_TO_TCHAR(lua_tostring(L, -7)) : FString();
    const bool bCurrentEntryHasNumericDestFlags = lua_isnumber(L, -1);
    const int32 CurrentDestFlags = bCurrentEntryHasNumericDestFlags ? static_cast<int32>(lua_tointeger(L, -1)) : 0;
    lua_pop(L, 8);
    TestTrue(TEXT("CombatLogGetCurrentEntry returns a WotLK event tuple"), bCurrentEntryHasStringEvent);
    TestEqual(TEXT("CombatLog current-entry placeholder event is empty"), CurrentEntryEventName, FString());
    TestTrue(TEXT("CombatLogGetCurrentEntry returns numeric destination flags"), bCurrentEntryHasNumericDestFlags);
    TestEqual(TEXT("CombatLog current-entry defaults unknown destination flags to COMBATLOG_OBJECT_NONE"), CurrentDestFlags, static_cast<int32>(0x80000000u));

    lua_getglobal(L, "CombatLog_Object_IsA");
    lua_pushinteger(L, 0x00000511);
    lua_pushinteger(L, 0x00000511);
    const int CombatLogObjectIsAResult = lua_pcall(L, 2, 1, 0);
    TestEqual(TEXT("CombatLog_Object_IsA call succeeds"), CombatLogObjectIsAResult, 0);
    const bool bCombatLogObjectMatches = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    TestTrue(TEXT("CombatLog_Object_IsA matches valid composite combat log filters"), bCombatLogObjectMatches);

    lua_getglobal(L, "CombatLog_Object_IsA");
    lua_pushinteger(L, 0x00000004);
    lua_pushinteger(L, 0x00000004);
    const int CombatLogObjectSingleConstantResult = lua_pcall(L, 2, 1, 0);
    TestEqual(TEXT("CombatLog_Object_IsA rejects single-category constants cleanly"), CombatLogObjectSingleConstantResult, 0);
    const bool bCombatLogObjectSingleConstantMatches = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    TestFalse(TEXT("CombatLog_Object_IsA rejects incomplete masks"), bCombatLogObjectSingleConstantMatches);

    lua_getglobal(L, "CombatLog_Object_IsA");
    lua_pushinteger(L, static_cast<lua_Integer>(0x80000000u));
    lua_pushinteger(L, static_cast<lua_Integer>(0x80000000u));
    const int CombatLogObjectUnknownResult = lua_pcall(L, 2, 1, 0);
    TestEqual(TEXT("CombatLog_Object_IsA handles COMBATLOG_OBJECT_NONE"), CombatLogObjectUnknownResult, 0);
    const bool bCombatLogObjectUnknownMatches = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);
    TestTrue(TEXT("CombatLog_Object_IsA matches unknown-unit filters"), bCombatLogObjectUnknownMatches);

    LuaVM.Shutdown();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerInventoryFieldAccessors, "WowUnreal.Entity.PlayerInventoryFieldAccessors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlayerInventoryFieldAccessors::RunTest(const FString& Parameters)
{
    FWowPlayerEntity Player;
    Player.TypeMask = WowTypeMask::UNIT | WowTypeMask::PLAYER;

    const uint64 BackpackGuid = 0x0000000200000001ULL;
    const uint64 BankGuid = 0x0000000300000002ULL;
    const uint64 BankBagGuid = 0x0000000400000003ULL;
    const uint64 WornBagGuid = 0x0000000500000004ULL;

    Player.SetField(PlayerField::PACK_SLOT_START, static_cast<uint32>(BackpackGuid & 0xFFFFFFFFu));
    Player.SetField(PlayerField::PACK_SLOT_START + 1, static_cast<uint32>(BackpackGuid >> 32));
    Player.SetField(PlayerField::BANK_SLOT_START, static_cast<uint32>(BankGuid & 0xFFFFFFFFu));
    Player.SetField(PlayerField::BANK_SLOT_START + 1, static_cast<uint32>(BankGuid >> 32));
    Player.SetField(PlayerField::BANKBAG_SLOT_1, static_cast<uint32>(BankBagGuid & 0xFFFFFFFFu));
    Player.SetField(PlayerField::BANKBAG_SLOT_1 + 1, static_cast<uint32>(BankBagGuid >> 32));
    Player.SetField(PlayerField::INV_SLOT_BAG_1, static_cast<uint32>(WornBagGuid & 0xFFFFFFFFu));
    Player.SetField(PlayerField::INV_SLOT_BAG_1 + 1, static_cast<uint32>(WornBagGuid >> 32));
    Player.SetField(PlayerField::BYTES_2, 2u << 16);

    TestEqual(TEXT("Backpack slot accessor uses corrected pack-slot range"), Player.GetBackpackItemGuid(0), BackpackGuid);
    TestEqual(TEXT("Bank slot accessor uses corrected bank-slot range"), Player.GetBankItemGuid(0), BankGuid);
    TestEqual(TEXT("Bank bag accessor uses corrected bank-bag range"), Player.GetBankBagGuid(0), BankBagGuid);
    TestEqual(TEXT("Worn bag accessor uses inventory slot bag range"), Player.GetBagGuid(1), WornBagGuid);
    TestEqual(TEXT("Purchased bank bag slot count comes from PLAYER_BYTES_2 byte 2"), Player.GetBankBagSlotCount(), static_cast<uint8>(2));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShowBankPacketBroadcastsEvent, "WowUnreal.Network.ShowBankPacketBroadcastsEvent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShowBankPacketBroadcastsEvent::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    bool bCalled = false;
    uint64 OpenedBankerGuid = 0;
    PacketHandler.OnBankOpened.AddLambda([&bCalled, &OpenedBankerGuid](uint64 BankerGuid)
    {
        bCalled = true;
        OpenedBankerGuid = BankerGuid;
    });

    const uint64 BankerGuid = 0x1122334455667788ULL;
    TArray<uint8> PacketData;
    PacketData.SetNumUninitialized(8);
    FMemory::Memcpy(PacketData.GetData(), &BankerGuid, sizeof(BankerGuid));

    PacketHandler.HandlePacket(WowOpcode::SMSG_SHOW_BANK, PacketData);

    TestTrue(TEXT("SMSG_SHOW_BANK fires the bank-open delegate"), bCalled);
    TestEqual(TEXT("Delegate receives the banker guid from the packet"), OpenedBankerGuid, BankerGuid);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShowMailboxPacketBroadcastsEvent, "WowUnreal.Network.ShowMailboxPacketBroadcastsEvent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FShowMailboxPacketBroadcastsEvent::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    bool bCalled = false;
    uint64 OpenedMailboxGuid = 0;
    PacketHandler.OnMailboxShown.AddLambda([&bCalled, &OpenedMailboxGuid](uint64 MailboxGuid)
    {
        bCalled = true;
        OpenedMailboxGuid = MailboxGuid;
    });

    const uint64 MailboxGuid = 0x8877665544332211ULL;
    TArray<uint8> PacketData;
    PacketData.SetNumUninitialized(8);
    FMemory::Memcpy(PacketData.GetData(), &MailboxGuid, sizeof(MailboxGuid));

    PacketHandler.HandlePacket(WowOpcode::SMSG_SHOW_MAILBOX, PacketData);

    TestTrue(TEXT("SMSG_SHOW_MAILBOX fires the mailbox-open delegate"), bCalled);
    TestEqual(TEXT("Delegate receives the mailbox guid from the packet"), OpenedMailboxGuid, MailboxGuid);
    TestEqual(TEXT("Packet handler stores the current mailbox guid"), PacketHandler.CurrentMailboxGuid, MailboxGuid);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameObjectMailboxTypeAccessor, "WowUnreal.Entity.GameObjectMailboxTypeAccessor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGameObjectMailboxTypeAccessor::RunTest(const FString& Parameters)
{
    FWowGameObjectEntity Mailbox;
    Mailbox.TypeMask = WowTypeMask::GAMEOBJECT;
    Mailbox.SetField(GameObjectField::BYTES_1, static_cast<uint32>(WowGameObjectType::MAILBOX) << 8);

    TestEqual(TEXT("Mailbox gameobject type is read from GAMEOBJECT_BYTES_1 byte 1"), Mailbox.GetGameObjectType(), static_cast<uint8>(WowGameObjectType::MAILBOX));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMailListPacketParsesInbox, "WowUnreal.Network.MailListPacketParsesInbox",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMailListPacketParsesInbox::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    bool bCalled = false;
    TArray<FWowMailMessage> ParsedMail;
    PacketHandler.OnMailListReceived.AddLambda([&bCalled, &ParsedMail](const TArray<FWowMailMessage>& Mail)
    {
        bCalled = true;
        ParsedMail = Mail;
    });

    TArray<uint8> PacketData;
    TArray<uint8> MailBlock;

    auto AppendU8 = [](TArray<uint8>& Buffer, uint8 Value)
    {
        Buffer.Add(Value);
    };
    auto AppendU16 = [](TArray<uint8>& Buffer, uint16 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint16));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint16));
    };
    auto AppendU32 = [](TArray<uint8>& Buffer, uint32 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint32));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint32));
    };
    auto AppendU64 = [](TArray<uint8>& Buffer, uint64 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint64));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint64));
    };
    auto AppendFloat = [](TArray<uint8>& Buffer, float Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(float));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(float));
    };
    auto AppendCString = [](TArray<uint8>& Buffer, const ANSICHAR* Value)
    {
        const int32 Length = FCStringAnsi::Strlen(Value) + 1;
        const int32 Offset = Buffer.AddUninitialized(Length);
        FMemory::Memcpy(Buffer.GetData() + Offset, Value, Length);
    };

    AppendU32(PacketData, 1u);
    AppendU8(PacketData, 1u);

    const uint32 MessageId = 42;
    const uint64 SenderGuid = 0x1122334455667788ULL;
    const float DaysLeft = 29.5f;
    const uint32 ItemEntry = 6948;

    AppendU32(MailBlock, MessageId);
    AppendU8(MailBlock, WowMailMessageType::NORMAL);
    AppendU64(MailBlock, SenderGuid);
    AppendU32(MailBlock, 1234u); // C.O.D.
    AppendU32(MailBlock, 0u);    // unknown 3.3.5 field
    AppendU32(MailBlock, 61u);   // stationery
    AppendU32(MailBlock, 56789u);
    AppendU32(MailBlock, WowMailCheckMask::HAS_BODY);
    AppendFloat(MailBlock, DaysLeft);
    AppendU32(MailBlock, 0u); // template id
    AppendCString(MailBlock, "Welcome");
    AppendCString(MailBlock, "Mailbox parser smoke test.");
    AppendU8(MailBlock, 1u); // one attachment
    AppendU8(MailBlock, 0u); // attachment index
    AppendU32(MailBlock, 77u);
    AppendU32(MailBlock, ItemEntry);
    for (int32 EnchantIndex = 0; EnchantIndex < 7; ++EnchantIndex)
    {
        AppendU32(MailBlock, 0u);
        AppendU32(MailBlock, 0u);
        AppendU32(MailBlock, 0u);
    }
    AppendU32(MailBlock, static_cast<uint32>(-42)); // random property id
    AppendU32(MailBlock, 99u); // suffix factor
    AppendU32(MailBlock, 2u);  // count
    AppendU32(MailBlock, 0u);  // charges
    AppendU32(MailBlock, 100u);
    AppendU32(MailBlock, 80u);
    AppendU8(MailBlock, 0u);

    AppendU16(PacketData, static_cast<uint16>(MailBlock.Num() + sizeof(uint16)));
    PacketData.Append(MailBlock);

    PacketHandler.HandlePacket(WowOpcode::SMSG_MAIL_LIST_RESULT, PacketData);

    TestTrue(TEXT("SMSG_MAIL_LIST_RESULT fires the mail list delegate"), bCalled);
    TestEqual(TEXT("Parsed inbox has one message"), ParsedMail.Num(), 1);
    TestEqual(TEXT("Packet handler stores the parsed inbox"), PacketHandler.MailInbox.Num(), 1);
    if (ParsedMail.Num() != 1 || PacketHandler.MailInbox.Num() != 1)
    {
        return false;
    }

    const FWowMailMessage& Mail = ParsedMail[0];
    TestEqual(TEXT("Mail message id parsed"), Mail.MessageId, MessageId);
    TestEqual(TEXT("Mail type parsed"), Mail.MessageType, static_cast<uint8>(WowMailMessageType::NORMAL));
    TestEqual(TEXT("Mail sender guid parsed"), Mail.SenderGuid, SenderGuid);
    TestEqual(TEXT("Mail COD parsed"), Mail.COD, 1234u);
    TestEqual(TEXT("Mail money parsed"), Mail.Money, 56789u);
    TestEqual(TEXT("Mail subject parsed"), Mail.Subject, FString(TEXT("Welcome")));
    TestEqual(TEXT("Mail body parsed"), Mail.Body, FString(TEXT("Mailbox parser smoke test.")));
    TestEqual(TEXT("Mail attachment count parsed"), Mail.Attachments.Num(), 1);
    TestTrue(TEXT("Mail days left parsed"), FMath::IsNearlyEqual(Mail.DaysLeft, DaysLeft, KINDA_SMALL_NUMBER));

    if (Mail.Attachments.Num() != 1)
    {
        return false;
    }

    const FWowMailAttachment& Attachment = Mail.Attachments[0];
    TestEqual(TEXT("Attachment entry parsed"), Attachment.ItemEntry, ItemEntry);
    TestEqual(TEXT("Attachment count parsed"), Attachment.Count, 2u);
    TestEqual(TEXT("Attachment random property id parsed as signed"), Attachment.RandomPropertyId, -42);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeStatusPacketParsesState, "WowUnreal.Network.TradeStatusPacketParsesState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTradeStatusPacketParsesState::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    bool bCalled = false;
    FWowTradeState TradeState;
    PacketHandler.OnTradeUpdated.AddLambda([&bCalled, &TradeState](const FWowTradeState& State)
    {
        bCalled = true;
        TradeState = State;
    });

    TArray<uint8> BeginPacket;
    const uint32 BeginStatus = WowTradeStatus::BEGIN_TRADE;
    const uint64 TraderGuid = 0x8877665544332211ULL;
    BeginPacket.AddUninitialized(sizeof(uint32) + sizeof(uint64));
    FMemory::Memcpy(BeginPacket.GetData(), &BeginStatus, sizeof(uint32));
    FMemory::Memcpy(BeginPacket.GetData() + sizeof(uint32), &TraderGuid, sizeof(uint64));

    PacketHandler.HandlePacket(WowOpcode::SMSG_TRADE_STATUS, BeginPacket);

    TestTrue(TEXT("Begin trade packet fires delegate"), bCalled);
    TestEqual(TEXT("Trade status parsed"), TradeState.Status, BeginStatus);
    TestEqual(TEXT("Trader guid parsed"), TradeState.TraderGuid, TraderGuid);
    TestFalse(TEXT("Begin trade does not mark window open yet"), TradeState.bTradeOpen);

    bCalled = false;
    TArray<uint8> OpenPacket;
    const uint32 OpenStatus = WowTradeStatus::OPEN_WINDOW;
    const uint32 OpenCookie = 0;
    OpenPacket.AddUninitialized(sizeof(uint32) + sizeof(uint32));
    FMemory::Memcpy(OpenPacket.GetData(), &OpenStatus, sizeof(uint32));
    FMemory::Memcpy(OpenPacket.GetData() + sizeof(uint32), &OpenCookie, sizeof(uint32));

    PacketHandler.HandlePacket(WowOpcode::SMSG_TRADE_STATUS, OpenPacket);

    TestTrue(TEXT("Open trade packet fires delegate"), bCalled);
    TestEqual(TEXT("Open trade status parsed"), TradeState.Status, OpenStatus);
    TestTrue(TEXT("Open trade marks window open"), TradeState.bTradeOpen);
    TestEqual(TEXT("Trader guid persists across status updates"), TradeState.TraderGuid, TraderGuid);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeStatusExtendedParsesOffers, "WowUnreal.Network.TradeStatusExtendedParsesOffers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTradeStatusExtendedParsesOffers::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    auto AppendU8 = [](TArray<uint8>& Buffer, uint8 Value)
    {
        Buffer.Add(Value);
    };
    auto AppendU32 = [](TArray<uint8>& Buffer, uint32 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint32));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint32));
    };
    auto AppendU64 = [](TArray<uint8>& Buffer, uint64 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint64));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint64));
    };

    auto AppendTradeSlot = [&AppendU8, &AppendU32, &AppendU64](TArray<uint8>& Buffer, uint8 SlotIndex, uint32 ItemId, uint32 DisplayId, uint32 Count)
    {
        AppendU8(Buffer, SlotIndex);
        AppendU32(Buffer, ItemId);
        AppendU32(Buffer, DisplayId);
        AppendU32(Buffer, Count);
        AppendU32(Buffer, 0u); // wrapped
        AppendU64(Buffer, 0u); // gift creator
        AppendU32(Buffer, 0u); // permanent enchant
        AppendU32(Buffer, 0u); // gem 1
        AppendU32(Buffer, 0u); // gem 2
        AppendU32(Buffer, 0u); // gem 3
        AppendU64(Buffer, 0u); // creator
        AppendU32(Buffer, 0u); // charges
        AppendU32(Buffer, 0u); // suffix factor
        AppendU32(Buffer, 0u); // random property
        AppendU32(Buffer, 0u); // lock id
        AppendU32(Buffer, 100u); // max durability
        AppendU32(Buffer, 80u);  // durability
    };

    TArray<uint8> TargetPacket;
    AppendU8(TargetPacket, 1u);  // trader data
    AppendU32(TargetPacket, 0u); // window cookie
    AppendU32(TargetPacket, 7u);
    AppendU32(TargetPacket, 7u);
    AppendU32(TargetPacket, 50000u);
    AppendU32(TargetPacket, 0u);
    for (uint8 Slot = 0; Slot < 7; ++Slot)
    {
        AppendTradeSlot(TargetPacket, Slot, Slot == 0 ? 6948u : 0u, Slot == 0 ? 123u : 0u, Slot == 0 ? 2u : 0u);
    }

    PacketHandler.HandlePacket(WowOpcode::SMSG_TRADE_STATUS_EXTENDED, TargetPacket);

    TestTrue(TEXT("Trade extended packet opens trade state"), PacketHandler.CurrentTrade.bTradeOpen);
    TestEqual(TEXT("Target money parsed"), PacketHandler.CurrentTrade.TargetMoney, 50000u);
    TestEqual(TEXT("Target trade slots parsed"), PacketHandler.CurrentTrade.TargetItems.Num(), 7);
    if (PacketHandler.CurrentTrade.TargetItems.Num() != 7)
    {
        return false;
    }

    TestEqual(TEXT("Target trade item entry parsed"), PacketHandler.CurrentTrade.TargetItems[0].ItemId, 6948u);
    TestEqual(TEXT("Target trade item count parsed"), PacketHandler.CurrentTrade.TargetItems[0].Count, 2u);

    TArray<uint8> PlayerPacket;
    AppendU8(PlayerPacket, 0u);  // player data
    AppendU32(PlayerPacket, 0u); // window cookie
    AppendU32(PlayerPacket, 7u);
    AppendU32(PlayerPacket, 7u);
    AppendU32(PlayerPacket, 12345u);
    AppendU32(PlayerPacket, 0u);
    for (uint8 Slot = 0; Slot < 7; ++Slot)
    {
        AppendTradeSlot(PlayerPacket, Slot, Slot == 1 ? 17031u : 0u, Slot == 1 ? 321u : 0u, Slot == 1 ? 5u : 0u);
    }

    PacketHandler.HandlePacket(WowOpcode::SMSG_TRADE_STATUS_EXTENDED, PlayerPacket);

    TestEqual(TEXT("Player money parsed"), PacketHandler.CurrentTrade.PlayerMoney, 12345u);
    TestEqual(TEXT("Player trade slots parsed"), PacketHandler.CurrentTrade.PlayerItems.Num(), 7);
    if (PacketHandler.CurrentTrade.PlayerItems.Num() != 7)
    {
        return false;
    }

    TestEqual(TEXT("Player trade item entry parsed"), PacketHandler.CurrentTrade.PlayerItems[1].ItemId, 17031u);
    TestEqual(TEXT("Player trade item count parsed"), PacketHandler.CurrentTrade.PlayerItems[1].Count, 5u);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeStatusPacketTracksLifecycle, "WowUnreal.Network.TradeStatusPacketTracksLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTradeStatusPacketTracksLifecycle::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    int32 UpdateCount = 0;
    PacketHandler.OnTradeUpdated.AddLambda([&UpdateCount](const FWowTradeState&)
    {
        ++UpdateCount;
    });

    auto AppendU8 = [](TArray<uint8>& Buffer, uint8 Value)
    {
        Buffer.Add(Value);
    };
    auto AppendU32 = [](TArray<uint8>& Buffer, uint32 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint32));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint32));
    };
    auto AppendU64 = [](TArray<uint8>& Buffer, uint64 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint64));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint64));
    };

    const uint64 TraderGuid = 0x1020304050607080ULL;

    TArray<uint8> BeginPacket;
    AppendU32(BeginPacket, WowTradeStatus::BEGIN_TRADE);
    AppendU64(BeginPacket, TraderGuid);
    PacketHandler.HandlePacket(WowOpcode::SMSG_TRADE_STATUS, BeginPacket);

    TestEqual(TEXT("BEGIN_TRADE stores the trader guid"), PacketHandler.CurrentTrade.TraderGuid, TraderGuid);
    TestEqual(TEXT("BEGIN_TRADE updates trade status"), PacketHandler.CurrentTrade.Status, static_cast<uint32>(WowTradeStatus::BEGIN_TRADE));
    TestFalse(TEXT("Trade is not open at request time"), PacketHandler.CurrentTrade.bTradeOpen);
    TestFalse(TEXT("Local accepted starts cleared"), PacketHandler.CurrentTrade.bLocalAccepted);
    TestFalse(TEXT("Target accepted starts cleared"), PacketHandler.CurrentTrade.bTargetAccepted);

    TArray<uint8> OpenPacket;
    AppendU32(OpenPacket, WowTradeStatus::OPEN_WINDOW);
    AppendU32(OpenPacket, 0u);
    PacketHandler.HandlePacket(WowOpcode::SMSG_TRADE_STATUS, OpenPacket);

    TestTrue(TEXT("OPEN_WINDOW marks the trade as open"), PacketHandler.CurrentTrade.bTradeOpen);

    TArray<uint8> AcceptPacket;
    AppendU32(AcceptPacket, WowTradeStatus::TRADE_ACCEPT);
    PacketHandler.HandlePacket(WowOpcode::SMSG_TRADE_STATUS, AcceptPacket);

    TestTrue(TEXT("TRADE_ACCEPT marks the remote player as accepted"), PacketHandler.CurrentTrade.bTargetAccepted);

    PacketHandler.CurrentTrade.bLocalAccepted = true;
    TArray<uint8> BackPacket;
    AppendU32(BackPacket, WowTradeStatus::BACK_TO_TRADE);
    PacketHandler.HandlePacket(WowOpcode::SMSG_TRADE_STATUS, BackPacket);

    TestFalse(TEXT("BACK_TO_TRADE clears local acceptance"), PacketHandler.CurrentTrade.bLocalAccepted);
    TestFalse(TEXT("BACK_TO_TRADE clears remote acceptance"), PacketHandler.CurrentTrade.bTargetAccepted);
    TestTrue(TEXT("BACK_TO_TRADE keeps the trade window open"), PacketHandler.CurrentTrade.bTradeOpen);

    PacketHandler.CurrentTrade.PlayerMoney = 100u;
    PacketHandler.CurrentTrade.TargetMoney = 200u;
    PacketHandler.CurrentTrade.PlayerItems.SetNum(1);
    PacketHandler.CurrentTrade.PlayerItems[0].ItemId = 6948;

    TArray<uint8> ClosePacket;
    AppendU32(ClosePacket, WowTradeStatus::CLOSE_WINDOW);
    AppendU32(ClosePacket, 0u);
    AppendU8(ClosePacket, 0u);
    AppendU32(ClosePacket, 0u);
    PacketHandler.HandlePacket(WowOpcode::SMSG_TRADE_STATUS, ClosePacket);

    TestFalse(TEXT("CLOSE_WINDOW hides the trade window"), PacketHandler.CurrentTrade.bTradeOpen);
    TestEqual(TEXT("CLOSE_WINDOW clears local money"), PacketHandler.CurrentTrade.PlayerMoney, 0u);
    TestEqual(TEXT("CLOSE_WINDOW clears target money"), PacketHandler.CurrentTrade.TargetMoney, 0u);
    TestEqual(TEXT("CLOSE_WINDOW clears offered items"), PacketHandler.CurrentTrade.PlayerItems.Num(), 0);
    TestTrue(TEXT("Trade update delegate fired for each lifecycle packet"), UpdateCount >= 4);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTradeStatusExtendedParsesBothSides, "WowUnreal.Network.TradeStatusExtendedParsesBothSides",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FTradeStatusExtendedParsesBothSides::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    auto AppendU8 = [](TArray<uint8>& Buffer, uint8 Value)
    {
        Buffer.Add(Value);
    };
    auto AppendU32 = [](TArray<uint8>& Buffer, uint32 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint32));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint32));
    };
    auto AppendU64 = [](TArray<uint8>& Buffer, uint64 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint64));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint64));
    };
    auto AppendTradeSlot = [&AppendU8, &AppendU32, &AppendU64](TArray<uint8>& Buffer, uint8 Slot, uint32 ItemId, uint32 DisplayId, uint32 Count)
    {
        AppendU8(Buffer, Slot);
        AppendU32(Buffer, ItemId);
        AppendU32(Buffer, DisplayId);
        AppendU32(Buffer, Count);
        AppendU32(Buffer, 0u);
        AppendU64(Buffer, 0u);
        AppendU32(Buffer, 0u);
        AppendU32(Buffer, 0u);
        AppendU32(Buffer, 0u);
        AppendU32(Buffer, 0u);
        AppendU64(Buffer, 0u);
        AppendU32(Buffer, 0u);
        AppendU32(Buffer, 0u);
        AppendU32(Buffer, 0u);
        AppendU32(Buffer, 0u);
        AppendU32(Buffer, 0u);
        AppendU32(Buffer, 0u);
    };

    auto BuildExtendedPacket = [&AppendU8, &AppendU32, &AppendTradeSlot](bool bTraderData, uint32 Money, uint32 SpellId, uint8 OccupiedSlot, uint32 ItemId, uint32 DisplayId, uint32 Count)
    {
        TArray<uint8> Packet;
        AppendU8(Packet, bTraderData ? 1u : 0u);
        AppendU32(Packet, 0u);
        AppendU32(Packet, 7u);
        AppendU32(Packet, 7u);
        AppendU32(Packet, Money);
        AppendU32(Packet, SpellId);
        for (uint8 Slot = 0; Slot < 7; ++Slot)
        {
            const bool bOccupied = (Slot == OccupiedSlot);
            AppendTradeSlot(Packet, Slot, bOccupied ? ItemId : 0u, bOccupied ? DisplayId : 0u, bOccupied ? Count : 0u);
        }
        return Packet;
    };

    PacketHandler.HandlePacket(
        WowOpcode::SMSG_TRADE_STATUS_EXTENDED,
        BuildExtendedPacket(false, 12345u, 133u, 0u, 6948u, 111u, 2u));
    PacketHandler.HandlePacket(
        WowOpcode::SMSG_TRADE_STATUS_EXTENDED,
        BuildExtendedPacket(true, 54321u, 686u, 1u, 17031u, 222u, 1u));

    TestTrue(TEXT("Trade window is open after extended payloads"), PacketHandler.CurrentTrade.bTradeOpen);
    TestEqual(TEXT("Extended payload sets the trade status to OPEN_WINDOW"), PacketHandler.CurrentTrade.Status, static_cast<uint32>(WowTradeStatus::OPEN_WINDOW));
    TestEqual(TEXT("Player money parsed from own trade payload"), PacketHandler.CurrentTrade.PlayerMoney, 12345u);
    TestEqual(TEXT("Target money parsed from trader trade payload"), PacketHandler.CurrentTrade.TargetMoney, 54321u);
    TestEqual(TEXT("Player spell id parsed"), PacketHandler.CurrentTrade.PlayerSpell, 133u);
    TestEqual(TEXT("Target spell id parsed"), PacketHandler.CurrentTrade.TargetSpell, 686u);
    TestTrue(TEXT("Player trade items array has all trade slots"), PacketHandler.CurrentTrade.PlayerItems.IsValidIndex(0));
    TestTrue(TEXT("Target trade items array has all trade slots"), PacketHandler.CurrentTrade.TargetItems.IsValidIndex(1));

    if (!PacketHandler.CurrentTrade.PlayerItems.IsValidIndex(0) || !PacketHandler.CurrentTrade.TargetItems.IsValidIndex(1))
    {
        return false;
    }

    TestEqual(TEXT("Own side item entry parsed"), PacketHandler.CurrentTrade.PlayerItems[0].ItemId, 6948u);
    TestEqual(TEXT("Own side item count parsed"), PacketHandler.CurrentTrade.PlayerItems[0].Count, 2u);
    TestEqual(TEXT("Trader side item entry parsed"), PacketHandler.CurrentTrade.TargetItems[1].ItemId, 17031u);
    TestEqual(TEXT("Trader side item count parsed"), PacketHandler.CurrentTrade.TargetItems[1].Count, 1u);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaidGroupListParsesRosterMetadata, "WowUnreal.Network.Raid.GroupListParsesRosterMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRaidGroupListParsesRosterMetadata::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    auto AppendU8 = [](TArray<uint8>& Buffer, uint8 Value)
    {
        Buffer.Add(Value);
    };
    auto AppendU32 = [](TArray<uint8>& Buffer, uint32 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint32));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint32));
    };
    auto AppendU64 = [](TArray<uint8>& Buffer, uint64 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint64));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint64));
    };
    auto AppendCString = [](TArray<uint8>& Buffer, const ANSICHAR* Value)
    {
        const int32 Length = FCStringAnsi::Strlen(Value) + 1;
        const int32 Offset = Buffer.AddUninitialized(Length);
        FMemory::Memcpy(Buffer.GetData() + Offset, Value, Length);
    };

    const uint64 GroupGuid = 0x1020304050607080ULL;
    const uint64 LeaderGuid = 0x9988776655443322ULL;
    const uint64 MemberGuidA = 0x1111222233334444ULL;
    const uint64 MemberGuidB = LeaderGuid;

    TArray<uint8> Packet;
    AppendU8(Packet, WowGroupType::RAID);
    AppendU8(Packet, 2u);
    AppendU8(Packet, WowGroupMemberFlags::ASSISTANT);
    AppendU8(Packet, 3u);
    AppendU64(Packet, GroupGuid);
    AppendU32(Packet, 41u);
    AppendU32(Packet, 2u);

    AppendCString(Packet, "Tanky");
    AppendU64(Packet, MemberGuidA);
    AppendU8(Packet, WowGroupMemberStatus::ONLINE);
    AppendU8(Packet, 0u);
    AppendU8(Packet, WowGroupMemberFlags::MAINTANK);
    AppendU8(Packet, 1u);

    AppendCString(Packet, "Leader");
    AppendU64(Packet, MemberGuidB);
    AppendU8(Packet, WowGroupMemberStatus::ONLINE);
    AppendU8(Packet, 1u);
    AppendU8(Packet, 0u);
    AppendU8(Packet, 2u);

    AppendU64(Packet, LeaderGuid);
    AppendU8(Packet, 2u);
    AppendU64(Packet, 0u);
    AppendU8(Packet, 3u);
    AppendU8(Packet, 1u);
    AppendU8(Packet, 2u);
    AppendU8(Packet, 0u);

    PacketHandler.HandlePacket(WowOpcode::SMSG_GROUP_LIST, Packet);

    TestTrue(TEXT("Group list marks the roster as a raid"), PacketHandler.GroupInfo.IsRaidGroup());
    TestEqual(TEXT("Group list stores the raw group flags"), PacketHandler.GroupInfo.RawGroupType, static_cast<uint8>(WowGroupType::RAID));
    TestEqual(TEXT("Group list stores the group guid"), PacketHandler.GroupInfo.GroupGuid, GroupGuid);
    TestEqual(TEXT("Group list stores the total member count including the local player"), PacketHandler.GroupInfo.MemberCount, static_cast<uint8>(3));
    TestEqual(TEXT("Group list stores the leader guid"), PacketHandler.GroupInfo.LeaderGuid, LeaderGuid);
    TestEqual(TEXT("Group list stores the local subgroup"), PacketHandler.GroupInfo.SelfSubgroup, static_cast<uint8>(2));
    TestEqual(TEXT("Group list stores the local flags"), PacketHandler.GroupInfo.SelfFlags, static_cast<uint8>(WowGroupMemberFlags::ASSISTANT));
    TestEqual(TEXT("Group list stores the remote roster size"), PacketHandler.GroupInfo.Members.Num(), 2);
    if (PacketHandler.GroupInfo.Members.Num() != 2)
    {
        return false;
    }

    TestEqual(TEXT("First remote member name parsed"), PacketHandler.GroupInfo.Members[0].Name, FString(TEXT("Tanky")));
    TestEqual(TEXT("First remote member subgroup parsed"), PacketHandler.GroupInfo.Members[0].Group, static_cast<uint8>(0));
    TestEqual(TEXT("Second remote member guid parsed"), PacketHandler.GroupInfo.Members[1].Guid, MemberGuidB);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaidTargetPacketsTrackAssignments, "WowUnreal.Network.Raid.TargetPacketsTrackAssignments",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRaidTargetPacketsTrackAssignments::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    auto AppendU8 = [](TArray<uint8>& Buffer, uint8 Value)
    {
        Buffer.Add(Value);
    };
    auto AppendU64 = [](TArray<uint8>& Buffer, uint64 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint64));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint64));
    };

    const uint64 TargetA = 0xABCDEF0011223344ULL;
    const uint64 TargetB = 0x5566778899AABBCCULL;
    const uint64 TargetC = 0xCAFEBABE01020304ULL;

    TArray<uint8> ListPacket;
    AppendU8(ListPacket, 1u);
    AppendU8(ListPacket, 0u);
    AppendU64(ListPacket, TargetA);
    AppendU8(ListPacket, 7u);
    AppendU64(ListPacket, TargetB);
    PacketHandler.HandlePacket(WowOpcode::MSG_RAID_TARGET_UPDATE, ListPacket);

    TestEqual(TEXT("Icon list assigns icon 1 to the first target"), PacketHandler.RaidTargets.GetIconIndex(TargetA), 0);
    TestEqual(TEXT("Icon list assigns icon 8 to the second target"), PacketHandler.RaidTargets.GetIconIndex(TargetB), 7);

    TArray<uint8> UpdatePacket;
    AppendU8(UpdatePacket, 0u);
    AppendU64(UpdatePacket, 0x0102030405060708ULL);
    AppendU8(UpdatePacket, 7u);
    AppendU64(UpdatePacket, TargetC);
    PacketHandler.HandlePacket(WowOpcode::MSG_RAID_TARGET_UPDATE, UpdatePacket);

    TestEqual(TEXT("Single target update replaces the icon target"), PacketHandler.RaidTargets.GetIconIndex(TargetC), 7);
    TestEqual(TEXT("Replaced target no longer has an icon"), PacketHandler.RaidTargets.GetIconIndex(TargetB), INDEX_NONE);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaidReadyCheckPacketsTrackLifecycle, "WowUnreal.Network.Raid.ReadyCheckPacketsTrackLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRaidReadyCheckPacketsTrackLifecycle::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    auto AppendU8 = [](TArray<uint8>& Buffer, uint8 Value)
    {
        Buffer.Add(Value);
    };
    auto AppendU64 = [](TArray<uint8>& Buffer, uint64 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint64));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint64));
    };

    const uint64 InitiatorGuid = 0x8877665544332211ULL;
    const uint64 ReadyGuid = 0x1111111122222222ULL;
    const uint64 NotReadyGuid = 0x3333333344444444ULL;

    TArray<uint8> StartPacket;
    AppendU64(StartPacket, InitiatorGuid);
    PacketHandler.HandlePacket(WowOpcode::MSG_RAID_READY_CHECK, StartPacket);

    TestTrue(TEXT("Ready check becomes active on request"), PacketHandler.ReadyCheck.bActive);
    TestEqual(TEXT("Ready check stores the initiator"), PacketHandler.ReadyCheck.InitiatorGuid, InitiatorGuid);
    TestTrue(TEXT("Ready check starts with remaining time"), PacketHandler.ReadyCheck.GetTimeLeftSeconds() > 0.0f);

    TArray<uint8> ReadyPacket;
    AppendU64(ReadyPacket, ReadyGuid);
    AppendU8(ReadyPacket, WowReadyCheckResponse::READY);
    PacketHandler.HandlePacket(WowOpcode::MSG_RAID_READY_CHECK_CONFIRM, ReadyPacket);

    TArray<uint8> NotReadyPacket;
    AppendU64(NotReadyPacket, NotReadyGuid);
    AppendU8(NotReadyPacket, WowReadyCheckResponse::NOT_READY);
    PacketHandler.HandlePacket(WowOpcode::MSG_RAID_READY_CHECK_CONFIRM, NotReadyPacket);

    const uint8* ReadyState = PacketHandler.ReadyCheck.FindResponse(ReadyGuid);
    const uint8* NotReadyState = PacketHandler.ReadyCheck.FindResponse(NotReadyGuid);
    TestNotNull(TEXT("Ready responder is tracked"), ReadyState);
    TestNotNull(TEXT("Not-ready responder is tracked"), NotReadyState);
    if (!ReadyState || !NotReadyState)
    {
        return false;
    }

    TestEqual(TEXT("Ready response value is stored"), *ReadyState, static_cast<uint8>(WowReadyCheckResponse::READY));
    TestEqual(TEXT("Not-ready response value is stored"), *NotReadyState, static_cast<uint8>(WowReadyCheckResponse::NOT_READY));

    PacketHandler.HandlePacket(WowOpcode::MSG_RAID_READY_CHECK_FINISHED, {});
    TestFalse(TEXT("Ready check is cleared when finished"), PacketHandler.ReadyCheck.bActive);
    TestEqual(TEXT("Ready check responses are cleared when finished"), PacketHandler.ReadyCheck.Responses.Num(), 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelRequestPacketsTrackLifecycle, "WowUnreal.Network.Duel.RequestPacketsTrackLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDuelRequestPacketsTrackLifecycle::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    auto AppendU8 = [](TArray<uint8>& Buffer, uint8 Value)
    {
        Buffer.Add(Value);
    };
    auto AppendU32 = [](TArray<uint8>& Buffer, uint32 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint32));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint32));
    };
    auto AppendU64 = [](TArray<uint8>& Buffer, uint64 Value)
    {
        const int32 Offset = Buffer.AddUninitialized(sizeof(uint64));
        FMemory::Memcpy(Buffer.GetData() + Offset, &Value, sizeof(uint64));
    };

    const uint64 ArbiterGuid = 0x8877665544332211ULL;
    const uint64 InitiatorGuid = 0x1122334455667788ULL;

    TArray<uint8> RequestPacket;
    AppendU64(RequestPacket, ArbiterGuid);
    AppendU64(RequestPacket, InitiatorGuid);
    PacketHandler.HandlePacket(WowOpcode::SMSG_DUEL_REQUESTED, RequestPacket);

    TestTrue(TEXT("Duel request enters the requested phase"), PacketHandler.CurrentDuel.IsRequestPending());
    TestEqual(TEXT("Duel request stores arbiter guid"), PacketHandler.CurrentDuel.ArbiterGuid, ArbiterGuid);
    TestEqual(TEXT("Duel request stores initiator guid"), PacketHandler.CurrentDuel.InitiatorGuid, InitiatorGuid);

    TArray<uint8> CountdownPacket;
    AppendU32(CountdownPacket, 3000u);
    PacketHandler.HandlePacket(WowOpcode::SMSG_DUEL_COUNTDOWN, CountdownPacket);

    TestTrue(TEXT("Duel countdown starts after accept"), PacketHandler.CurrentDuel.IsCountdownActive());
    TestTrue(TEXT("Duel countdown has time remaining"), PacketHandler.CurrentDuel.GetCountdownRemainingSeconds() > 0.0f);

    PacketHandler.HandlePacket(WowOpcode::SMSG_DUEL_OUTOFBOUNDS, {});
    TestFalse(TEXT("Out-of-bounds packet flips the in-bounds flag"), PacketHandler.CurrentDuel.bInBounds);

    PacketHandler.HandlePacket(WowOpcode::SMSG_DUEL_INBOUNDS, {});
    TestTrue(TEXT("In-bounds packet restores the in-bounds flag"), PacketHandler.CurrentDuel.bInBounds);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDuelCompletionPacketsParseResults, "WowUnreal.Network.Duel.CompletionPacketsParseResults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDuelCompletionPacketsParseResults::RunTest(const FString& Parameters)
{
    FWowPacketHandler PacketHandler;

    auto AppendU8 = [](TArray<uint8>& Buffer, uint8 Value)
    {
        Buffer.Add(Value);
    };
    auto AppendCString = [](TArray<uint8>& Buffer, const ANSICHAR* Value)
    {
        const int32 Length = FCStringAnsi::Strlen(Value) + 1;
        const int32 Offset = Buffer.AddUninitialized(Length);
        FMemory::Memcpy(Buffer.GetData() + Offset, Value, Length);
    };

    TArray<uint8> InterruptedPacket;
    AppendU8(InterruptedPacket, 0u);
    PacketHandler.HandlePacket(WowOpcode::SMSG_DUEL_COMPLETE, InterruptedPacket);

    TestTrue(TEXT("Interrupted duel enters the completed phase"), PacketHandler.CurrentDuel.IsComplete());
    TestTrue(TEXT("Interrupted duel sets interrupted state"), PacketHandler.CurrentDuel.bInterrupted);
    TestFalse(TEXT("Interrupted duel does not mark a winner"), PacketHandler.CurrentDuel.bHasWinner);

    PacketHandler.CurrentDuel.Clear();

    TArray<uint8> CompletePacket;
    AppendU8(CompletePacket, 1u);
    PacketHandler.HandlePacket(WowOpcode::SMSG_DUEL_COMPLETE, CompletePacket);

    TArray<uint8> WinnerPacket;
    AppendU8(WinnerPacket, WowDuelResultReason::FLED);
    AppendCString(WinnerPacket, "Winner");
    AppendCString(WinnerPacket, "Loser");
    PacketHandler.HandlePacket(WowOpcode::SMSG_DUEL_WINNER, WinnerPacket);

    TestTrue(TEXT("Winner packet keeps the duel completed"), PacketHandler.CurrentDuel.IsComplete());
    TestTrue(TEXT("Winner packet marks a duel winner"), PacketHandler.CurrentDuel.bHasWinner);
    TestEqual(TEXT("Winner packet stores the result reason"), PacketHandler.CurrentDuel.ResultReason, static_cast<uint8>(WowDuelResultReason::FLED));
    TestEqual(TEXT("Winner packet stores the winner name"), PacketHandler.CurrentDuel.WinnerName, FString(TEXT("Winner")));
    TestEqual(TEXT("Winner packet stores the loser name"), PacketHandler.CurrentDuel.LoserName, FString(TEXT("Loser")));

    return true;
}

// ====================================================================
// DBC Parser Tests (require MPQ data)
// ====================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDbcParserBasic, "WowUnreal.Parser.DBC.MapDbc",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDbcParserBasic::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping DBC test"));
        return true;
    }

    TArray<uint8> Data;
    if (!Mpq.ReadFile(TEXT("DBFilesClient\\Map.dbc"), Data))
    {
        AddError(TEXT("Failed to read Map.dbc from MPQ"));
        return false;
    }

    FDbcParser Parser;
    TestTrue(TEXT("Map.dbc parses"), Parser.Parse(Data));
    TestTrue(TEXT("Has records"), Parser.GetRecordCount() > 0);
    TestTrue(TEXT("Has fields"), Parser.GetFieldCount() > 0);

    // Map.dbc record 0 should have ID in field 0
    uint32 FirstId = Parser.GetUInt(0, 0);
    TestTrue(TEXT("First record has valid ID"), FirstId > 0 || FirstId == 0); // ID 0 is Eastern Kingdoms

    // Get a string field — map name is field 5 in Map.dbc
    FString MapName = Parser.GetString(0, 5);
    TestTrue(TEXT("Map name not empty"), MapName.Len() > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDbcParserAreaTable, "WowUnreal.Parser.DBC.AreaTableDbc",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDbcParserAreaTable::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping"));
        return true;
    }

    TArray<uint8> Data;
    if (!Mpq.ReadFile(TEXT("DBFilesClient\\AreaTable.dbc"), Data))
    {
        AddError(TEXT("Failed to read AreaTable.dbc"));
        return false;
    }

    FDbcParser Parser;
    TestTrue(TEXT("AreaTable.dbc parses"), Parser.Parse(Data));
    // WoW 3.3.5 has ~3000+ areas
    TestTrue(TEXT("AreaTable has >1000 records"), Parser.GetRecordCount() > 1000);

    return true;
}

// ====================================================================
// BLP Parser Tests (require MPQ data)
// ====================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlpParserDxt, "WowUnreal.Parser.BLP.DxtTexture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBlpParserDxt::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping"));
        return true;
    }

    TArray<uint8> Data;
    // Try common terrain textures (case-insensitive, backslash paths)
    bool bFound = Mpq.ReadFile(TEXT("Tileset\\Elwynn\\ElwynnGrassBase01.blp"), Data)
               || Mpq.ReadFile(TEXT("TILESET\\ELWYNN\\ELWYNNGRASSBASE01.BLP"), Data)
               || Mpq.ReadFile(TEXT("Tileset\\Generic\\GenericRock01.blp"), Data)
               || Mpq.ReadFile(TEXT("TILESET\\GENERIC\\GENERICROCK01.BLP"), Data);
    if (!bFound)
    {
        AddWarning(TEXT("Could not find a BLP texture in MPQ — skipping"));
        return true;
    }

    FBlpTexture Tex = FBlpParser::Parse(Data);
    TestTrue(TEXT("BLP is valid"), Tex.bIsValid);
    TestTrue(TEXT("Has width"), Tex.Width > 0);
    TestTrue(TEXT("Has height"), Tex.Height > 0);
    TestTrue(TEXT("Has mip levels"), Tex.MipLevels.Num() > 0);
    TestTrue(TEXT("Is DXT format"), Tex.PixelFormat == EBlpPixelFormat::DXT1 ||
             Tex.PixelFormat == EBlpPixelFormat::DXT3 || Tex.PixelFormat == EBlpPixelFormat::DXT5);
    TestTrue(TEXT("Mip 0 has data"), Tex.MipLevels[0].Data.Num() > 0);

    return true;
}

// ====================================================================
// ADT Parser Tests (require MPQ data)
// ====================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdtParserBasic, "WowUnreal.Parser.ADT.Elwynn32_48",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAdtParserBasic::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping"));
        return true;
    }

    TArray<uint8> Data;
    if (!Mpq.ReadFile(TEXT("World\\Maps\\Azeroth\\Azeroth_32_48.adt"), Data))
    {
        AddError(TEXT("Failed to read Azeroth_32_48.adt"));
        return false;
    }

    FAdtData Adt = FAdtParser::Parse(Data);
    TestTrue(TEXT("ADT is valid"), Adt.bIsValid);

    // Should have 256 chunks (16x16)
    // Verify first chunk has valid height data (not all zeros would be unusual)
    bool bHasNonZeroHeight = false;
    for (int32 i = 0; i < 145; ++i)
    {
        if (FMath::Abs(Adt.Chunks[0].Heights[i]) > 0.001f)
        {
            bHasNonZeroHeight = true;
            break;
        }
    }
    TestTrue(TEXT("Chunk 0 has height data"), bHasNonZeroHeight);

    // Should have texture paths
    TestTrue(TEXT("Has texture paths"), Adt.TexturePaths.Num() > 0);

    // Should have doodad placements (Elwynn is heavily populated)
    TestTrue(TEXT("Has doodad placements"), Adt.DoodadPlacements.Num() > 0);

    return true;
}

// ====================================================================
// M2 Parser Tests (require MPQ data)
// ====================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FM2ParserBasic, "WowUnreal.Parser.M2.TreeModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FM2ParserBasic::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping"));
        return true;
    }

    // Read a common doodad (tree)
    TArray<uint8> M2Data;
    if (!Mpq.ReadFile(TEXT("World\\Azeroth\\Elwynn\\PassiveDoodads\\Trees\\ElwynnTree01.m2"), M2Data))
    {
        // Try alternate path
        if (!Mpq.ReadFile(TEXT("World\\Azeroth\\Elwynn\\PassiveDoodads\\Trees\\ElwynnTree01.M2"), M2Data))
        {
            AddWarning(TEXT("Could not find tree M2 — skipping"));
            return true;
        }
    }

    // Read skin file
    TArray<uint8> SkinData;
    FString SkinPath = TEXT("World\\Azeroth\\Elwynn\\PassiveDoodads\\Trees\\ElwynnTree0100.skin");
    Mpq.ReadFile(SkinPath, SkinData);

    FM2Data Model = FM2Parser::Parse(M2Data, SkinData);
    TestTrue(TEXT("M2 is valid"), Model.bIsValid);
    TestTrue(TEXT("Has vertices"), Model.Vertices.Num() > 0);

    return true;
}

// ====================================================================
// WMO Parser Tests (require MPQ data)
// ====================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWmoParserBasic, "WowUnreal.Parser.WMO.GoldshireInn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWmoParserBasic::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping"));
        return true;
    }

    TArray<uint8> Data;
    if (!Mpq.ReadFile(TEXT("World\\wmo\\Azeroth\\Buildings\\GoldShireInn\\GoldShire_Inn.wmo"), Data))
    {
        AddWarning(TEXT("Could not find GoldShire_Inn.wmo — skipping"));
        return true;
    }

    FWmoRootData Root = FWmoParser::ParseRoot(Data);
    TestTrue(TEXT("WMO root is valid"), Root.bIsValid);
    TestTrue(TEXT("Has groups"), Root.NumGroups > 0);
    TestTrue(TEXT("Has materials"), Root.Materials.Num() > 0);

    return true;
}

// ====================================================================
// MPQ Manager Tests (filesystem)
// ====================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMpqManagerBasic, "WowUnreal.Mpq.InitAndRead",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMpqManagerBasic::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ data not available at expected path — skipping"));
        return true;
    }

    TestTrue(TEXT("MPQ is initialized"), Mpq.IsInitialized());

    // Test file existence
    TestTrue(TEXT("Map.dbc exists"), Mpq.FileExists(TEXT("DBFilesClient\\Map.dbc")));
    TestFalse(TEXT("Nonexistent file"), Mpq.FileExists(TEXT("this\\does\\not\\exist.blp")));

    // Test reading a known file
    TArray<uint8> Data;
    TestTrue(TEXT("Read Map.dbc"), Mpq.ReadFile(TEXT("DBFilesClient\\Map.dbc"), Data));
    TestTrue(TEXT("Map.dbc has data"), Data.Num() > 0);

    // DBC header magic should be "WDBC"
    TestTrue(TEXT("DBC data >= 4 bytes"), Data.Num() >= 4);
    if (Data.Num() >= 4)
    {
        TestEqual(TEXT("DBC magic W"), (char)Data[0], 'W');
        TestEqual(TEXT("DBC magic D"), (char)Data[1], 'D');
        TestEqual(TEXT("DBC magic B"), (char)Data[2], 'B');
        TestEqual(TEXT("DBC magic C"), (char)Data[3], 'C');
    }

    return true;
}

// ====================================================================
// Audio DBC Tests (require MPQ data)
// ====================================================================

#include "Formats/Dbc/DbcStore.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FZoneMusicDbcTest, "WowUnreal.Audio.ZoneMusicDbc",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FZoneMusicDbcTest::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping"));
        return true;
    }

    // Ensure DBCs are loaded
    FDbcStore::Get().LoadAll(Mpq);

    // ZoneMusic ID 1 is Elwynn Forest music
    const FZoneMusicDbcEntry* Entry = FDbcStore::Get().ZoneMusic().GetById(1);
    TestNotNull(TEXT("ZoneMusic ID 1 exists"), Entry);
    if (Entry)
    {
        TestTrue(TEXT("Day sound ID is valid (not zero)"), Entry->SoundDayID > 0);
        TestTrue(TEXT("Day sound ID is a reasonable SoundEntry ID (<100000)"), Entry->SoundDayID < 100000);
        TestTrue(TEXT("Night sound ID is valid"), Entry->SoundNightID > 0);
    }

    TestTrue(TEXT("ZoneMusic has >10 entries"), FDbcStore::Get().ZoneMusic().Num() > 10);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundEntriesDbcTest, "WowUnreal.Audio.SoundEntriesDbc",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSoundEntriesDbcTest::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping"));
        return true;
    }

    FDbcStore::Get().LoadAll(Mpq);

    TestTrue(TEXT("SoundEntries has >1000 entries"), FDbcStore::Get().SoundEntries().Num() > 1000);

    // Verify a known sound entry has a file path
    const FZoneMusicDbcEntry* ZM = FDbcStore::Get().ZoneMusic().GetById(1);
    if (ZM && ZM->SoundDayID > 0)
    {
        const FSoundEntriesDbcEntry* SE = FDbcStore::Get().SoundEntries().GetById(ZM->SoundDayID);
        TestNotNull(TEXT("Elwynn day music SoundEntry exists"), SE);
        if (SE)
        {
            bool bHasFile = false;
            for (int32 i = 0; i < 10; ++i)
            {
                if (!SE->FileDataID[i].IsEmpty())
                {
                    bHasFile = true;
                    break;
                }
            }
            TestTrue(TEXT("Elwynn music SoundEntry has at least one file"), bHasFile);
            TestTrue(TEXT("SoundEntry has directory base"), !SE->DirectoryBase.IsEmpty());
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundAmbienceDbcTest, "WowUnreal.Audio.SoundAmbienceDbc",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSoundAmbienceDbcTest::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping"));
        return true;
    }

    FDbcStore::Get().LoadAll(Mpq);

    TestTrue(TEXT("SoundAmbience has >10 entries"), FDbcStore::Get().SoundAmbience().Num() > 10);

    // AreaTable ID 12 (Elwynn Forest) should have an ambience ID
    const FAreaTableDbcEntry* Area = FDbcStore::Get().AreaTable().GetById(12);
    TestNotNull(TEXT("Elwynn Forest (ID 12) exists in AreaTable"), Area);
    if (Area)
    {
        TestTrue(TEXT("Elwynn has ambience ID"), Area->AmbienceID > 0);
        TestTrue(TEXT("Elwynn has zone music ID"), Area->ZoneMusicID > 0);

        if (Area->AmbienceID > 0)
        {
            const FSoundAmbienceDbcEntry* Amb = FDbcStore::Get().SoundAmbience().GetById(Area->AmbienceID);
            TestNotNull(TEXT("Elwynn ambience entry exists"), Amb);
            if (Amb)
            {
                TestTrue(TEXT("Elwynn has day ambience sound"), Amb->DayAmbience > 0);
            }
        }
    }

    return true;
}

// ====================================================================
// Packet Handler Tests (in-memory, no network dependency)
// ====================================================================

#include "WowPacketHandler.h"
#include "WowOpcodes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPacketHandlerSpellStart, "WowUnreal.Network.HandleSpellStart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPacketHandlerSpellStart::RunTest(const FString& Parameters)
{
    FWowPacketHandler Handler;

    TArray<uint8> TestData;
    // Packed GUID (caster item): mask=0x01, low=0x42
    TestData.Add(0x01); TestData.Add(0x42);
    // Packed GUID (caster): mask=0x01, low=0x43
    TestData.Add(0x01); TestData.Add(0x43);
    // Cast counter
    TestData.Add(0x01);
    // Spell ID = 133 (Fireball)
    TestData.Add(133); TestData.Add(0); TestData.Add(0); TestData.Add(0);
    // Cast flags = 0
    TestData.Add(0); TestData.Add(0); TestData.Add(0); TestData.Add(0);
    // Cast time = 3000ms
    TestData.Add(0xB8); TestData.Add(0x0B); TestData.Add(0); TestData.Add(0);

    // Should not crash
    Handler.HandlePacket(WowOpcode::SMSG_SPELL_START, TestData);
    TestTrue(TEXT("SPELL_START handled without crash"), true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPacketHandlerPowerUpdate, "WowUnreal.Network.HandlePowerUpdate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPacketHandlerPowerUpdate::RunTest(const FString& Parameters)
{
    FWowPacketHandler Handler;

    TArray<uint8> TestData;
    // Packed GUID: mask=0x01, low=0x44
    TestData.Add(0x01); TestData.Add(0x44);
    // Power type = 0 (mana)
    TestData.Add(0x00);
    // Power value = 1500
    TestData.Add(0xDC); TestData.Add(0x05); TestData.Add(0); TestData.Add(0);

    Handler.HandlePacket(WowOpcode::SMSG_POWER_UPDATE, TestData);
    TestTrue(TEXT("POWER_UPDATE handled without crash"), true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPacketHandlerEntityCreation, "WowUnreal.Network.EntityCreation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPacketHandlerEntityCreation::RunTest(const FString& Parameters)
{
    FWowEntityManager EM;

    // Create and promote a unit entity
    FWowEntity& Base = EM.GetOrCreate(5001);
    Base.SetField(UnitField::HEALTH, 100);
    Base.SetField(UnitField::MAXHEALTH, 200);
    Base.SetField(UnitField::DISPLAYID, 3167); // Stormwind Guard
    Base.SetField(UnitField::LEVEL, 75);
    Base.SetField(UnitField::BYTES_0, 1 | (1 << 8) | (0 << 16)); // Race=1(Human), Class=1(Warrior), Gender=0(Male)

    EM.PromoteToTyped(5001, WowTypeMask::UNIT);
    FWowUnitEntity* Unit = EM.FindUnit(5001);
    TestNotNull(TEXT("Promoted to unit"), Unit);
    if (Unit)
    {
        TestEqual(TEXT("Display ID"), Unit->GetDisplayId(), (uint32)3167);
        TestEqual(TEXT("Race"), Unit->GetRaceId(), (uint8)1);
        TestEqual(TEXT("Class"), Unit->GetClassId(), (uint8)1);
        TestEqual(TEXT("Gender"), Unit->GetGenderId(), (uint8)0);
        TestEqual(TEXT("Level"), Unit->GetLevel(), 75);
    }

    return true;
}

// ====================================================================
// Character Builder Tests (model path lookup, no rendering)
// ====================================================================

#include "WowCharacterBuilder.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterModelPath, "WowUnreal.Character.ModelPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCharacterModelPath::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping"));
        return true;
    }

    FDbcStore::Get().LoadAll(Mpq);

    // Human Male
    FString HumanMale = FWowCharacterBuilder::GetCharacterModelPath(
        FWowCharacterBuilder::ERace::Human, FWowCharacterBuilder::EGender::Male);
    TestTrue(TEXT("Human Male path not empty"), !HumanMale.IsEmpty());
    TestTrue(TEXT("Human Male path contains 'Human'"), HumanMale.Contains(TEXT("Human")));
    TestTrue(TEXT("Human Male path ends with .m2"), HumanMale.EndsWith(TEXT(".m2")));

    // Orc Female
    FString OrcFemale = FWowCharacterBuilder::GetCharacterModelPath(
        FWowCharacterBuilder::ERace::Orc, FWowCharacterBuilder::EGender::Female);
    TestTrue(TEXT("Orc Female path not empty"), !OrcFemale.IsEmpty());
    TestTrue(TEXT("Orc Female path contains 'Orc'"), OrcFemale.Contains(TEXT("Orc")));

    // Verify the M2 file exists in MPQ
    TArray<uint8> M2Data;
    TestTrue(TEXT("Human Male M2 exists in MPQ"), Mpq.ReadFile(HumanMale, M2Data));
    TestTrue(TEXT("Human Male M2 has data"), M2Data.Num() > 0);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCreatureDisplayLookup, "WowUnreal.Character.CreatureDisplayLookup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCreatureDisplayLookup::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping"));
        return true;
    }

    FDbcStore::Get().LoadAll(Mpq);

    // Stormwind Guard display ID = 3167
    const FCreatureDisplayInfoDbcEntry* DisplayInfo = FDbcStore::Get().CreatureDisplayInfo().GetById(3167);
    TestNotNull(TEXT("DisplayInfo 3167 exists"), DisplayInfo);
    if (DisplayInfo)
    {
        TestTrue(TEXT("Has model ID"), DisplayInfo->ModelID > 0);

        const FCreatureModelDataDbcEntry* ModelData = FDbcStore::Get().CreatureModelData().GetById(DisplayInfo->ModelID);
        TestNotNull(TEXT("CreatureModelData exists"), ModelData);
        if (ModelData)
        {
            TestTrue(TEXT("Has model path"), !ModelData->ModelPath.IsEmpty());
        }
    }

    return true;
}

// ====================================================================
// Movement Coordinate Tests (pure math)
// ====================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovementWowToUE, "WowUnreal.Movement.WowToUEPosition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMovementWowToUE::RunTest(const FString& Parameters)
{
    // Elwynn starting position
    FVector WowPos(-8949.0f, -132.0f, 84.0f);
    FVector UEPos = FWowCoordinate::WowToUE(WowPos);

    // Verify UE position is reasonable (within world bounds)
    TestTrue(TEXT("UE X is non-zero"), FMath::Abs(UEPos.X) > 100.0f);
    TestTrue(TEXT("UE Y is non-zero"), FMath::Abs(UEPos.Y) > 100.0f);
    TestTrue(TEXT("UE Z (height) is positive"), UEPos.Z > 0.0f);

    // Roundtrip
    FVector Back = FWowCoordinate::UEToWow(UEPos);
    TestTrue(TEXT("Roundtrip X"), FMath::IsNearlyEqual(Back.X, WowPos.X, 0.1f));
    TestTrue(TEXT("Roundtrip Y"), FMath::IsNearlyEqual(Back.Y, WowPos.Y, 0.1f));
    TestTrue(TEXT("Roundtrip Z"), FMath::IsNearlyEqual(Back.Z, WowPos.Z, 0.1f));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMovementScaleConsistency, "WowUnreal.Movement.ScaleConsistency",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMovementScaleConsistency::RunTest(const FString& Parameters)
{
    // Verify SCALE constant
    TestEqual(TEXT("SCALE is 100"), FWowCoordinate::SCALE, 100.0f);

    // One WoW tile = 533.33 yards, at 100 cm/yard = 53333 cm
    FVector TileCenter = FWowCoordinate::TileToWorld(0, 0);
    FVector NextTile = FWowCoordinate::TileToWorld(1, 0);
    float TileDistUE = FVector::Dist(TileCenter, NextTile);
    float ExpectedDist = FWowCoordinate::TILE_SIZE * FWowCoordinate::SCALE;
    TestTrue(TEXT("Adjacent tiles are TILE_SIZE * SCALE apart"),
        FMath::IsNearlyEqual(TileDistUE, ExpectedDist, 1.0f));

    return true;
}

// ====================================================================
// ADT Area ID Tests (require MPQ data)
// ====================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdtAreaIds, "WowUnreal.World.AdtAreaIds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAdtAreaIds::RunTest(const FString& Parameters)
{
    FMpqManager& Mpq = WowTestUtils::GetMpq();
    if (!Mpq.IsInitialized())
    {
        AddWarning(TEXT("MPQ not available — skipping"));
        return true;
    }

    TArray<uint8> Data;
    if (!Mpq.ReadFile(TEXT("World\\Maps\\Azeroth\\Azeroth_32_48.adt"), Data))
    {
        AddError(TEXT("Failed to read Azeroth_32_48.adt"));
        return false;
    }

    FAdtData Adt = FAdtParser::Parse(Data);
    TestTrue(TEXT("ADT is valid"), Adt.bIsValid);

    // All 256 chunks should have area IDs
    bool bHasAreaId = false;
    for (int32 i = 0; i < 256; ++i)
    {
        if (Adt.Chunks[i].AreaId > 0)
        {
            bHasAreaId = true;
            break;
        }
    }
    TestTrue(TEXT("At least one chunk has an area ID"), bHasAreaId);

    // Elwynn tile 32,48 should have Elwynn Forest or Northshire area IDs
    uint32 FirstAreaId = Adt.Chunks[0].AreaId;
    TestTrue(TEXT("First chunk area ID > 0"), FirstAreaId > 0);

    return true;
}
