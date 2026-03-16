#pragma once
#include "CoreMinimal.h"
#include "Formats/Dbc/MapDbc.h"
#include "Formats/Dbc/AreaTableDbc.h"
#include "Formats/Dbc/LightDbc.h"
#include "Formats/Dbc/LightParamsDbc.h"
#include "Formats/Dbc/LightIntParamsDbc.h"
#include "Formats/Dbc/LiquidTypeDbc.h"
#include "Formats/Dbc/AnimationDataDbc.h"
#include "Formats/Dbc/ChrRacesDbc.h"
#include "Formats/Dbc/CharSectionsDbc.h"
#include "Formats/Dbc/CreatureDisplayInfoDbc.h"
#include "Formats/Dbc/CreatureModelDataDbc.h"
#include "Formats/Dbc/ItemDisplayInfoDbc.h"
#include "Formats/Dbc/SpellDbc.h"
#include "Formats/Dbc/SpellVisualDbc.h"
#include "Formats/Dbc/SpellVisualKitDbc.h"
#include "Formats/Dbc/SoundEntriesDbc.h"
#include "Formats/Dbc/LoadingScreensDbc.h"
#include "Formats/Dbc/GroundEffectTextureDbc.h"
#include "Formats/Dbc/EmotesTextDbc.h"
#include "Formats/Dbc/TalentDbc.h"
#include "Formats/Dbc/LightFloatParamsDbc.h"
#include "Formats/Dbc/TalentTabDbc.h"
#include "Formats/Dbc/ZoneMusicDbc.h"
#include "Formats/Dbc/SoundAmbienceDbc.h"
#include "Formats/Dbc/CharComponentTextureLayoutsDbc.h"
#include "Formats/Dbc/CharComponentTextureSectionsDbc.h"
#include "Formats/Dbc/CharHairGeosetsDbc.h"
#include "Formats/Dbc/CharacterFacialHairStylesDbc.h"
#include "Formats/Dbc/CharStartOutfitDbc.h"

class FMpqManager;

class WOWDATA_API FDbcStore
{
public:
    static FDbcStore& Get();

    bool LoadAll(FMpqManager& Mpq);

    FMapDbc& Maps() { return MapDbc; }
    FAreaTableDbc& AreaTable() { return AreaTableDbc; }
    FLightDbc& Lights() { return LightDbc; }
    FLightParamsDbc& LightParams() { return LightParamsDbc; }
    FLightIntParamsDbc& LightIntParams() { return LightIntParamsDbc; }
    FLightFloatParamsDbc& LightFloatParams() { return LightFloatParamsDbc; }
    FLiquidTypeDbc& LiquidTypes() { return LiquidTypeDbc; }
    FAnimationDataDbc& AnimationData() { return AnimationDataDbc; }
    FChrRacesDbc& ChrRaces() { return ChrRacesDbc; }
    FCharSectionsDbc& CharSections() { return CharSectionsDbc; }
    FCreatureDisplayInfoDbc& CreatureDisplayInfo() { return CreatureDisplayInfoDbc; }
    FCreatureModelDataDbc& CreatureModelData() { return CreatureModelDataDbc; }
    FItemDisplayInfoDbc& ItemDisplayInfo() { return ItemDisplayInfoDbc; }
    FSpellDbc& Spells() { return SpellDbc; }
    FSpellVisualDbc& SpellVisuals() { return SpellVisualDbc; }
    FSpellVisualKitDbc& SpellVisualKits() { return SpellVisualKitDbc; }
    FSoundEntriesDbc& SoundEntries() { return SoundEntriesDbc; }
    FLoadingScreensDbc& LoadingScreens() { return LoadingScreensDbc; }
    FGroundEffectTextureDbc& GroundEffectTextures() { return GroundEffectTextureDbc; }
    FEmotesTextDbc& EmotesText() { return EmotesTextDbc; }
    FTalentDbc& Talents() { return TalentDbc; }
    FTalentTabDbc& TalentTabs() { return TalentTabDbc; }
    FZoneMusicDbc& ZoneMusic() { return ZoneMusicDbc; }
    FSoundAmbienceDbc& SoundAmbience() { return SoundAmbienceDbc; }
    FCharComponentTextureLayoutsDbc& CharComponentTextureLayouts() { return CharComponentTextureLayoutsDbc; }
    FCharComponentTextureSectionsDbc& CharComponentTextureSections() { return CharComponentTextureSectionsDbc; }
    FCharHairGeosetsDbc& CharHairGeosets() { return CharHairGeosetsDbc; }
    FCharacterFacialHairStylesDbc& CharacterFacialHairStyles() { return CharacterFacialHairStylesDbc; }
    FCharStartOutfitDbc& CharStartOutfit() { return CharStartOutfitDbc; }

    const FMapDbc& Maps() const { return MapDbc; }
    const FAreaTableDbc& AreaTable() const { return AreaTableDbc; }
    const FLightDbc& Lights() const { return LightDbc; }
    const FLightParamsDbc& LightParams() const { return LightParamsDbc; }
    const FLightIntParamsDbc& LightIntParams() const { return LightIntParamsDbc; }
    const FLightFloatParamsDbc& LightFloatParams() const { return LightFloatParamsDbc; }
    const FLiquidTypeDbc& LiquidTypes() const { return LiquidTypeDbc; }
    const FAnimationDataDbc& AnimationData() const { return AnimationDataDbc; }
    const FChrRacesDbc& ChrRaces() const { return ChrRacesDbc; }
    const FCharSectionsDbc& CharSections() const { return CharSectionsDbc; }
    const FCreatureDisplayInfoDbc& CreatureDisplayInfo() const { return CreatureDisplayInfoDbc; }
    const FCreatureModelDataDbc& CreatureModelData() const { return CreatureModelDataDbc; }
    const FItemDisplayInfoDbc& ItemDisplayInfo() const { return ItemDisplayInfoDbc; }
    const FSpellDbc& Spells() const { return SpellDbc; }
    const FSpellVisualDbc& SpellVisuals() const { return SpellVisualDbc; }
    const FSpellVisualKitDbc& SpellVisualKits() const { return SpellVisualKitDbc; }
    const FSoundEntriesDbc& SoundEntries() const { return SoundEntriesDbc; }
    const FLoadingScreensDbc& LoadingScreens() const { return LoadingScreensDbc; }
    const FGroundEffectTextureDbc& GroundEffectTextures() const { return GroundEffectTextureDbc; }
    const FEmotesTextDbc& EmotesText() const { return EmotesTextDbc; }
    const FTalentDbc& Talents() const { return TalentDbc; }
    const FTalentTabDbc& TalentTabs() const { return TalentTabDbc; }
    const FZoneMusicDbc& ZoneMusic() const { return ZoneMusicDbc; }
    const FSoundAmbienceDbc& SoundAmbience() const { return SoundAmbienceDbc; }
    const FCharComponentTextureLayoutsDbc& CharComponentTextureLayouts() const { return CharComponentTextureLayoutsDbc; }
    const FCharComponentTextureSectionsDbc& CharComponentTextureSections() const { return CharComponentTextureSectionsDbc; }
    const FCharHairGeosetsDbc& CharHairGeosets() const { return CharHairGeosetsDbc; }
    const FCharacterFacialHairStylesDbc& CharacterFacialHairStyles() const { return CharacterFacialHairStylesDbc; }
    const FCharStartOutfitDbc& CharStartOutfit() const { return CharStartOutfitDbc; }

    bool IsLoaded() const { return bLoaded; }

private:
    FMapDbc MapDbc;
    FAreaTableDbc AreaTableDbc;
    FLightDbc LightDbc;
    FLightParamsDbc LightParamsDbc;
    FLightIntParamsDbc LightIntParamsDbc;
    FLightFloatParamsDbc LightFloatParamsDbc;
    FLiquidTypeDbc LiquidTypeDbc;
    FAnimationDataDbc AnimationDataDbc;
    FChrRacesDbc ChrRacesDbc;
    FCharSectionsDbc CharSectionsDbc;
    FCreatureDisplayInfoDbc CreatureDisplayInfoDbc;
    FCreatureModelDataDbc CreatureModelDataDbc;
    FItemDisplayInfoDbc ItemDisplayInfoDbc;
    FSpellDbc SpellDbc;
    FSpellVisualDbc SpellVisualDbc;
    FSpellVisualKitDbc SpellVisualKitDbc;
    FSoundEntriesDbc SoundEntriesDbc;
    FLoadingScreensDbc LoadingScreensDbc;
    FGroundEffectTextureDbc GroundEffectTextureDbc;
    FEmotesTextDbc EmotesTextDbc;
    FTalentDbc TalentDbc;
    FTalentTabDbc TalentTabDbc;
    FZoneMusicDbc ZoneMusicDbc;
    FSoundAmbienceDbc SoundAmbienceDbc;
    FCharComponentTextureLayoutsDbc CharComponentTextureLayoutsDbc;
    FCharComponentTextureSectionsDbc CharComponentTextureSectionsDbc;
    FCharHairGeosetsDbc CharHairGeosetsDbc;
    FCharacterFacialHairStylesDbc CharacterFacialHairStylesDbc;
    FCharStartOutfitDbc CharStartOutfitDbc;
    bool bLoaded = false;
};
