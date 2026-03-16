#if WITH_EDITOR

#include "WowMapCreatorCommandlet.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/WorldSettings.h"
#include "FileHelpers.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowMapCreator, Log, All);

int32 UWowMapCreatorCommandlet::Main(const FString& Params)
{
    UE_LOG(LogWowMapCreator, Log, TEXT("=== WowMapCreator Commandlet ==="));

    struct FMapDef
    {
        FString Name;
        FString GameMode;
    };

    TArray<FMapDef> Maps = {
        { TEXT("WowWorld"),      TEXT("/Script/WowUnreal.WowViewerGameMode") },
        { TEXT("TerrainTest"),   TEXT("/Script/WowUnreal.WowTestGameMode") },
        { TEXT("CharacterTest"), TEXT("/Script/WowUnreal.WowTestGameMode") },
        { TEXT("AnimationTest"), TEXT("/Script/WowUnreal.WowTestGameMode") },
        { TEXT("MobTest"),       TEXT("/Script/WowUnreal.WowTestGameMode") },
        { TEXT("WmoTest"),       TEXT("/Script/WowUnreal.WowTestGameMode") },
        { TEXT("UITest"),        TEXT("/Script/WowUnreal.WowTestGameMode") },
        { TEXT("NetworkTest"),   TEXT("/Script/WowUnreal.WowTestGameMode") },
        { TEXT("StreamingTest"), TEXT("/Script/WowUnreal.WowTestGameMode") },
        { TEXT("ModelViewer"),   TEXT("/Script/WowUnreal.WowModelViewerGameMode") },
    };

    int32 Created = 0;
    for (const FMapDef& Map : Maps)
    {
        if (CreateMap(Map.Name, Map.GameMode))
        {
            ++Created;
        }
    }

    UE_LOG(LogWowMapCreator, Log, TEXT("Created %d/%d maps"), Created, Maps.Num());
    return 0;
}

bool UWowMapCreatorCommandlet::CreateMap(const FString& MapName, const FString& GameModeClassPath)
{
    FString PackagePath = FString::Printf(TEXT("/Game/Maps/%s"), *MapName);
    FString FilePath = FPaths::ProjectContentDir() / TEXT("Maps") / MapName + TEXT(".umap");

    // Skip if already exists
    if (FPaths::FileExists(FilePath))
    {
        UE_LOG(LogWowMapCreator, Log, TEXT("  SKIP: %s already exists"), *MapName);
        return false;
    }

    UE_LOG(LogWowMapCreator, Log, TEXT("  Creating: %s → %s"), *MapName, *FilePath);

    // Create package
    UPackage* Package = CreatePackage(*PackagePath);
    if (!Package)
    {
        UE_LOG(LogWowMapCreator, Error, TEXT("  FAIL: Could not create package for %s"), *MapName);
        return false;
    }
    Package->FullyLoad();

    // Create world
    UWorld* NewWorld = UWorld::CreateWorld(EWorldType::None, false, FName(*MapName), Package);
    if (!NewWorld)
    {
        UE_LOG(LogWowMapCreator, Error, TEXT("  FAIL: Could not create world for %s"), *MapName);
        return false;
    }

    // Mark world and all sub-objects as standalone so SavePackage accepts them
    NewWorld->SetFlags(RF_Standalone | RF_Public);
    if (NewWorld->PersistentLevel)
    {
        NewWorld->PersistentLevel->SetFlags(RF_Standalone | RF_Public);
    }

    // Set GameMode override in WorldSettings
    AWorldSettings* Settings = NewWorld->GetWorldSettings();
    if (Settings)
    {
        Settings->SetFlags(RF_Standalone | RF_Public);

        UClass* GMClass = LoadClass<AGameModeBase>(nullptr, *GameModeClassPath);
        if (GMClass)
        {
            Settings->DefaultGameMode = GMClass;
            UE_LOG(LogWowMapCreator, Log, TEXT("  Set GameMode: %s"), *GameModeClassPath);
        }
        else
        {
            UE_LOG(LogWowMapCreator, Warning, TEXT("  Could not load GameMode class: %s"), *GameModeClassPath);
        }
    }

    // Save
    FString AbsFilePath = FPaths::ConvertRelativePathToFull(FilePath);
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Standalone;
    UPackage::SavePackage(Package, NewWorld, *AbsFilePath, SaveArgs);

    // Verify it was saved
    if (FPaths::FileExists(FilePath))
    {
        UE_LOG(LogWowMapCreator, Log, TEXT("  OK: %s saved (%lld bytes)"), *MapName, IFileManager::Get().FileSize(*FilePath));
        return true;
    }

    UE_LOG(LogWowMapCreator, Error, TEXT("  FAIL: %s was not saved"), *MapName);
    return false;
}

#endif // WITH_EDITOR
