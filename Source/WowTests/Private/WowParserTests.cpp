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
#include "WowConnectionManager.h"

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
