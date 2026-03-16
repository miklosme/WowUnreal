#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/Texture2D.h"
#include "WowCursorManager.generated.h"

class FMpqManager;
class FWowAssetCache;

/**
 * Cursor types from WoW Interface/Cursor/ directory
 */
UENUM(BlueprintType)
enum class EWowCursorType : uint8
{
    Point,          // Default cursor
    Attack,         // When hovering attackable target
    Speak,          // When hovering NPC
    Buy,            // Vendor
    Quest,          // Quest giver
    Inspect,        // Inspectable
    Pickup,         // Lootable
    UnableAttack,   // Cannot attack
    UnablePoint     // Cannot interact
};

/**
 * Manages WoW custom cursors loaded from MPQ
 */
UCLASS(BlueprintType)
class WOWUNREAL_API UWowCursorManager : public UObject
{
    GENERATED_BODY()

public:
    UWowCursorManager();

    /** Initialize with MPQ manager and asset cache */
    void Initialize(FMpqManager* Mpq, FWowAssetCache* Cache);

    /** Load all cursor textures from MPQ */
    void LoadCursors();

    /** Set the current cursor type */
    UFUNCTION(BlueprintCallable, Category = "Cursor")
    void SetCursorType(EWowCursorType CursorType);

    /** Get cursor texture for a specific type */
    UFUNCTION(BlueprintCallable, Category = "Cursor")
    UTexture2D* GetCursorTexture(EWowCursorType CursorType) const;

private:
    /** Load a single cursor texture */
    UTexture2D* LoadCursorTexture(const FString& CursorName);

    /** Get the filename for a cursor type */
    FString GetCursorFilename(EWowCursorType CursorType) const;

    /** MPQ manager for loading files */
    FMpqManager* MpqManager = nullptr;

    /** Asset cache for texture caching */
    FWowAssetCache* AssetCache = nullptr;

    /** Loaded cursor textures */
    UPROPERTY()
    TMap<EWowCursorType, TObjectPtr<UTexture2D>> CursorTextures;

    /** Currently active cursor type */
    UPROPERTY()
    EWowCursorType CurrentCursorType = EWowCursorType::Point;
};