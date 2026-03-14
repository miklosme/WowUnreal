#pragma once
#include "CoreMinimal.h"

class FMpqManager;
class FWowAssetCache;
class USkeletalMeshComponent;
class AActor;
class UWorld;

/**
 * Builds and spawns WoW character models from race/gender/customization data.
 * Uses ChrRaces.dbc to map race+gender to M2 model paths, then loads the skeletal mesh.
 */
class WOWASSETS_API FWowCharacterBuilder
{
public:
    /** Race IDs from ChrRaces.dbc */
    enum class ERace : uint8
    {
        Human = 1, Orc = 2, Dwarf = 3, NightElf = 4, Undead = 5,
        Tauren = 6, Gnome = 7, Troll = 8, BloodElf = 10, Draenei = 11
    };

    enum class EGender : uint8 { Male = 0, Female = 1 };

    /** Get the M2 model path for a race/gender combo using ChrRaces.dbc */
    static FString GetCharacterModelPath(ERace Race, EGender Gender);

    /**
     * Spawn a character actor with skeletal mesh + idle animation.
     * Returns the spawned actor, or nullptr on failure.
     */
    static AActor* SpawnCharacter(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
        ERace Race, EGender Gender, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);

    /** Spawn a creature/NPC by display ID from CreatureDisplayInfo.dbc */
    static AActor* SpawnCreatureByDisplayId(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
        uint32 DisplayId, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);

private:
    /** Internal: load M2, build skeleton + mesh + anims, spawn actor */
    static AActor* SpawnM2Actor(UWorld* World, FMpqManager* Mpq, FWowAssetCache* Cache,
        const FString& ModelPath, const FVector& Location, const FRotator& Rotation, float Scale = 1.0f);
};
