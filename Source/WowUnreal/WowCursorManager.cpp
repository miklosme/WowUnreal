#include "WowCursorManager.h"
#include "WowTerrainMaterial.h" // For LoadBlpTexture function
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowCursor, Log, All);

UWowCursorManager::UWowCursorManager()
{
    CurrentCursorType = EWowCursorType::Point;
}

void UWowCursorManager::Initialize(FMpqManager* Mpq, FWowAssetCache* Cache)
{
    MpqManager = Mpq;
    AssetCache = Cache;

    if (!MpqManager)
    {
        UE_LOG(LogWowCursor, Error, TEXT("Failed to initialize WowCursorManager: MpqManager is null"));
        return;
    }

    UE_LOG(LogWowCursor, Log, TEXT("WowCursorManager initialized"));
}

void UWowCursorManager::LoadCursors()
{
    if (!MpqManager)
    {
        UE_LOG(LogWowCursor, Error, TEXT("Cannot load cursors: MpqManager is null"));
        return;
    }

    // Load all cursor types
    TArray<EWowCursorType> CursorTypes = {
        EWowCursorType::Point,
        EWowCursorType::Attack,
        EWowCursorType::Speak,
        EWowCursorType::Buy,
        EWowCursorType::Quest,
        EWowCursorType::Inspect,
        EWowCursorType::Pickup,
        EWowCursorType::UnableAttack,
        EWowCursorType::UnablePoint
    };

    int32 LoadedCount = 0;
    for (EWowCursorType CursorType : CursorTypes)
    {
        FString CursorName = GetCursorFilename(CursorType);
        UTexture2D* CursorTexture = LoadCursorTexture(CursorName);

        if (CursorTexture)
        {
            CursorTextures.Add(CursorType, CursorTexture);
            LoadedCount++;
            UE_LOG(LogWowCursor, Log, TEXT("Loaded cursor: %s"), *CursorName);
        }
        else
        {
            UE_LOG(LogWowCursor, Warning, TEXT("Failed to load cursor: %s"), *CursorName);
        }
    }

    UE_LOG(LogWowCursor, Log, TEXT("Loaded %d/%d cursor textures"), LoadedCount, CursorTypes.Num());

    // Set default cursor
    SetCursorType(EWowCursorType::Point);
}

void UWowCursorManager::SetCursorType(EWowCursorType CursorType)
{
    CurrentCursorType = CursorType;

    UTexture2D* CursorTexture = GetCursorTexture(CursorType);
    if (!CursorTexture)
    {
        UE_LOG(LogWowCursor, Warning, TEXT("No texture loaded for cursor type %d"), (int32)CursorType);
        return;
    }

    // For now, we'll just log that we're changing the cursor
    // Implementing actual hardware cursor change in UE5 requires platform-specific code
    // or using a software cursor widget system
    UE_LOG(LogWowCursor, Log, TEXT("Set cursor type to: %s"), *GetCursorFilename(CursorType));

    // TODO: Implement actual cursor change
    // This would involve either:
    // 1. Using FSlateApplication::Get().GetPlatformCursor() for hardware cursors
    // 2. Using a software cursor widget that follows the mouse
    // 3. Using UE5's built-in cursor system with custom cursor widgets
}

UTexture2D* UWowCursorManager::GetCursorTexture(EWowCursorType CursorType) const
{
    const TObjectPtr<UTexture2D>* Found = CursorTextures.Find(CursorType);
    return Found ? *Found : nullptr;
}

UTexture2D* UWowCursorManager::LoadCursorTexture(const FString& CursorName)
{
    if (!MpqManager)
    {
        return nullptr;
    }

    FString CursorPath = FString::Printf(TEXT("Interface/Cursor/%s.blp"), *CursorName);

    // Use the existing BLP texture loading function from WowTerrainMaterial
    // This function handles MPQ reading, BLP parsing, and texture creation
    return FWowTerrainMaterial::LoadBlpTexture(CursorPath, MpqManager, AssetCache);
}

FString UWowCursorManager::GetCursorFilename(EWowCursorType CursorType) const
{
    switch (CursorType)
    {
        case EWowCursorType::Point:         return TEXT("Point");
        case EWowCursorType::Attack:        return TEXT("Attack");
        case EWowCursorType::Speak:         return TEXT("Speak");
        case EWowCursorType::Buy:           return TEXT("Buy");
        case EWowCursorType::Quest:         return TEXT("Quest");
        case EWowCursorType::Inspect:       return TEXT("Inspect");
        case EWowCursorType::Pickup:        return TEXT("Pickup");
        case EWowCursorType::UnableAttack:  return TEXT("UnableAttack");
        case EWowCursorType::UnablePoint:   return TEXT("UnablePoint");
        default:                            return TEXT("Point");
    }
}