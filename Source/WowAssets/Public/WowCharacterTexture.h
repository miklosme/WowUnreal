#pragma once
#include "CoreMinimal.h"

class UTexture2D;
class FMpqManager;
class FWowAssetCache;

/**
 * Composites character textures from CharSections.dbc layers.
 * Bakes skin + face + facial hair + underwear into a single body texture.
 */
class WOWASSETS_API FWowCharacterTexture
{
public:
    /** Section types from CharSections.dbc */
    enum class ESectionType : uint32
    {
        Skin = 0,
        Face = 1,
        FacialHair = 2,
        Hair = 3,
        Underwear = 4
    };

    struct FCustomization
    {
        uint32 RaceId = 1;
        uint32 Gender = 0; // 0=male, 1=female
        uint32 SkinColor = 0;
        uint32 FaceVariation = 0;
        uint32 HairStyle = 0;
        uint32 HairColor = 0;
        uint32 FacialHairStyle = 0;
    };

    /**
     * Build a composite character texture by layering CharSections textures.
     * Returns the base skin texture if compositing fails.
     */
    static UTexture2D* BuildCompositeTexture(FMpqManager* Mpq, FWowAssetCache* Cache,
        const FCustomization& Customization);

    /** Look up a CharSections texture path for the given parameters */
    static FString GetSectionTexture(uint32 RaceId, uint32 Gender, ESectionType Type,
        uint32 Variation, uint32 Color);
};
