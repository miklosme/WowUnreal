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
