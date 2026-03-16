#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "WowCharacterBuilder.h"

DECLARE_DELEGATE(FOnModelViewerChanged);
DECLARE_DELEGATE_OneParam(FOnCreatureSpawn, uint32 /*DisplayId*/);

/**
 * Slate UI panel for the WoW Model Viewer.
 * Race/gender cycle selectors, customization options, equipment fields, creature spawn.
 */
class SWowModelViewerWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SWowModelViewerWidget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Called when any character parameter changes (race, gender, customization) */
    FOnModelViewerChanged OnCharacterChanged;

    /** Called when the user clicks Spawn Creature */
    FOnCreatureSpawn OnCreatureSpawn;

    /** Current state accessors */
    FWowCharacterBuilder::ERace GetSelectedRace() const;
    FWowCharacterBuilder::EGender GetSelectedGender() const;
    uint32 GetSkinColor() const { return SkinColor; }
    uint32 GetFaceVariation() const { return FaceVariation; }
    uint32 GetHairStyle() const { return HairStyle; }
    uint32 GetHairColor() const { return HairColor; }
    uint32 GetFacialHairStyle() const { return FacialHairStyle; }
    uint32 GetWeaponDisplayId() const;
    uint32 GetCreatureDisplayId() const;

    void SetStatusText(const FString& Text);

private:
    /** Build a cycle row with label, < >, and value display */
    TSharedRef<SHorizontalBox> MakeCycleRow(const FString& Label, TSharedPtr<STextBlock>& ValueText,
        FOnClicked OnPrev, FOnClicked OnNext);

    /** Build a text field row */
    TSharedRef<SHorizontalBox> MakeTextField(const FString& Label, TSharedPtr<SEditableTextBox>& TextField);

    void UpdateCustomizationRanges();
    void NotifyChanged();

    // Race/gender
    int32 RaceIndex = 0;
    int32 GenderIndex = 0; // 0=Male, 1=Female
    TSharedPtr<STextBlock> RaceText;
    TSharedPtr<STextBlock> GenderText;

    // Customization
    uint32 SkinColor = 0;
    uint32 FaceVariation = 0;
    uint32 HairStyle = 0;
    uint32 HairColor = 0;
    uint32 FacialHairStyle = 0;
    uint32 MaxSkinColors = 1;
    uint32 MaxFaces = 1;
    uint32 MaxHairStyles = 1;
    uint32 MaxHairColors = 1;
    uint32 MaxFacialHairStyles = 1;
    TSharedPtr<STextBlock> SkinText;
    TSharedPtr<STextBlock> FaceText;
    TSharedPtr<STextBlock> HairStyleText;
    TSharedPtr<STextBlock> HairColorText;
    TSharedPtr<STextBlock> FacialHairText;

    // Equipment
    TSharedPtr<SEditableTextBox> WeaponField;

    // Creature
    TSharedPtr<SEditableTextBox> CreatureField;

    // Status
    TSharedPtr<STextBlock> StatusLabel;
};
