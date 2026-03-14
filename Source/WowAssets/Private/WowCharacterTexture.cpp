#include "WowCharacterTexture.h"
#include "WowTextureFactory.h"
#include "WowAssetCache.h"
#include "Mpq/MpqManager.h"
#include "Formats/BlpParser.h"
#include "Formats/BlpTypes.h"
#include "Formats/Dbc/DbcStore.h"
#include "Engine/Texture2D.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowCharTex, Log, All);

FString FWowCharacterTexture::GetSectionTexture(uint32 RaceId, uint32 Gender, ESectionType Type,
    uint32 Variation, uint32 Color)
{
    const FCharSectionsDbc& CharSections = FDbcStore::Get().CharSections();

    for (const FCharSectionsDbcEntry& Entry : CharSections.GetAll())
    {
        if (Entry.RaceID == RaceId &&
            Entry.SexID == Gender &&
            Entry.Type == static_cast<uint32>(Type) &&
            Entry.Variation == Variation &&
            Entry.Color == Color)
        {
            // Return first non-empty texture path
            for (int32 i = 0; i < 3; i++)
            {
                if (!Entry.Textures[i].IsEmpty())
                {
                    return Entry.Textures[i];
                }
            }
        }
    }

    return FString();
}

UTexture2D* FWowCharacterTexture::BuildCompositeTexture(FMpqManager* Mpq, FWowAssetCache* Cache,
    const FCustomization& Customization)
{
    if (!Mpq) return nullptr;

    // Get base skin texture path
    FString SkinPath = GetSectionTexture(Customization.RaceId, Customization.Gender,
        ESectionType::Skin, Customization.SkinColor, 0);

    if (SkinPath.IsEmpty())
    {
        UE_LOG(LogWowCharTex, Warning, TEXT("No skin texture found for Race=%d Gender=%d SkinColor=%d"),
            Customization.RaceId, Customization.Gender, Customization.SkinColor);
        return nullptr;
    }

    // For now, return the base skin texture without compositing
    // Full compositing (face overlay, underwear, etc.) requires pixel-level blending
    // which we'll implement when equipment rendering needs it
    FString CacheKey = FString::Printf(TEXT("CharTex_%d_%d_%d_%d"),
        Customization.RaceId, Customization.Gender, Customization.SkinColor, Customization.FaceVariation);

    UTexture2D* CachedTex = Cache ? Cache->FindTexture(CacheKey) : nullptr;
    if (CachedTex) return CachedTex;

    // Load the base skin BLP texture
    TArray<uint8> BlpData;
    if (!Mpq->ReadFile(SkinPath, BlpData))
    {
        UE_LOG(LogWowCharTex, Warning, TEXT("Failed to read skin texture: %s"), *SkinPath);
        return nullptr;
    }

    FBlpTexture Blp = FBlpParser::Parse(BlpData);
    if (!Blp.bIsValid)
    {
        UE_LOG(LogWowCharTex, Warning, TEXT("Failed to parse skin BLP: %s"), *SkinPath);
        return nullptr;
    }

    UTexture2D* SkinTex = FWowTextureFactory::CreateTexture(Blp, CacheKey);
    if (SkinTex && Cache)
    {
        Cache->CacheTexture(CacheKey, SkinTex);
    }

    UE_LOG(LogWowCharTex, Log, TEXT("Built character texture: %s (%dx%d)"),
        *SkinPath, Blp.Width, Blp.Height);

    return SkinTex;
}
