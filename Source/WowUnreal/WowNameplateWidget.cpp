#include "WowNameplateWidget.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"

void UWowNameplateWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Widget binding will be handled by UMG reflection system
    // NameText and HealthBar will be automatically bound if they exist in a blueprint
}

void UWowNameplateWidget::SetupNameplate(const FString& EntityName, int32 Health, int32 MaxHealth, bool bIsHostile)
{
    bIsHostileEntity = bIsHostile;

    // Set entity name
    if (NameText)
    {
        NameText->SetText(FText::FromString(EntityName));

        // Color coding: red for hostile, green for friendly, yellow for neutral
        FLinearColor NameColor = bIsHostile ? FLinearColor::Red : FLinearColor::Green;
        if (!bIsHostile && EntityName.StartsWith(TEXT("NPC")))
        {
            NameColor = FLinearColor::Yellow; // Neutral NPCs
        }
        NameText->SetColorAndOpacity(NameColor);
    }

    // Set health bar
    UpdateHealth(Health, MaxHealth);
}

void UWowNameplateWidget::UpdateHealth(int32 Health, int32 MaxHealth)
{
    if (HealthBar && MaxHealth > 0)
    {
        float HealthPercent = FMath::Clamp(static_cast<float>(Health) / static_cast<float>(MaxHealth), 0.0f, 1.0f);
        HealthBar->SetPercent(HealthPercent);

        // Color health bar: red at low health, green at full health
        FLinearColor HealthColor;
        if (HealthPercent > 0.6f)
        {
            HealthColor = FLinearColor::Green;
        }
        else if (HealthPercent > 0.3f)
        {
            HealthColor = FLinearColor::Yellow;
        }
        else
        {
            HealthColor = FLinearColor::Red;
        }

        HealthBar->SetFillColorAndOpacity(HealthColor);
    }
}