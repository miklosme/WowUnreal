#pragma once
#include "CoreMinimal.h"

class FMpqManager;
class FWowAssetCache;
class USkeletalMeshComponent;
class AActor;
struct FM2Data;

/**
 * Loads and attaches equipment (weapons, shoulders, helmets) to character models.
 * Uses ItemDisplayInfo.dbc to find model paths, then loads M2 models and
 * parents them to attachment points on the character skeleton.
 */
class WOWASSETS_API FWowEquipmentManager
{
public:
    /** Well-known attachment point IDs from M2 format */
    enum class EAttachmentPoint : uint32
    {
        RightHand = 0,
        LeftHand = 1,
        Back = 2,
        RightShoulder = 5,
        LeftShoulder = 6,
        Helmet = 11,
        Shield = 26
    };

    /**
     * Attach a weapon or equipment model to a character's attachment point.
     * Loads the item's M2 model and parents it to the specified bone.
     * @param ItemDisplayId  ItemDisplayInfo.dbc ID
     * @param AttachPoint    Which attachment point to use
     * @param CharacterMesh  The character's skeletal mesh component
     * @param CharacterM2    The character's parsed M2 data (for attachment bone lookup)
     * @return The spawned static/skeletal mesh component, or nullptr
     */
    static USceneComponent* AttachEquipment(FMpqManager* Mpq, FWowAssetCache* Cache,
        uint32 ItemDisplayId, EAttachmentPoint AttachPoint,
        USkeletalMeshComponent* CharacterMesh, const FM2Data& CharacterM2);

    /** Get the bone name for a given attachment point from M2 data */
    static FName GetAttachmentBoneName(const FM2Data& M2Data, EAttachmentPoint AttachPoint);

    /** Resolve a display ID into a concrete M2/BLP pair for a specific attachment point. */
    static bool ResolveEquipmentPaths(FMpqManager* Mpq, uint32 ItemDisplayId, EAttachmentPoint AttachPoint,
        FString& OutModelPath, FString* OutTexturePath = nullptr);
};
