#include "WowFloatingText.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UWowFloatingText::UWowFloatingText()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;

    // Create text render component
    TextRenderComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextRender"));
    if (TextRenderComponent)
    {
        TextRenderComponent->SetupAttachment(this);
        TextRenderComponent->SetWorldSize(48.0f);
        TextRenderComponent->SetTextRenderColor(FColor::White);
        TextRenderComponent->SetHorizontalAlignment(EHTA_Center);
        TextRenderComponent->SetVerticalAlignment(EVRTA_TextCenter);
        // Face the camera
        TextRenderComponent->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
    }
}

void UWowFloatingText::BeginPlay()
{
    Super::BeginPlay();
    StartLocation = GetComponentLocation();
}

void UWowFloatingText::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsFloating)
    {
        UpdateFloating(DeltaTime);
    }
}

UWowFloatingText* UWowFloatingText::SpawnFloatingText(AActor* TargetActor, const FWowFloatingTextInfo& TextInfo)
{
    if (!TargetActor || !TargetActor->GetWorld())
    {
        return nullptr;
    }

    // Create a temporary actor to hold the floating text component
    AActor* FloatingTextActor = TargetActor->GetWorld()->SpawnActor<AActor>();
    if (!FloatingTextActor)
    {
        return nullptr;
    }

    // Position above the target actor
    FVector SpawnLocation = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 200.0f);
    FloatingTextActor->SetActorLocation(SpawnLocation);

    // Add floating text component
    UWowFloatingText* FloatingTextComponent = NewObject<UWowFloatingText>(FloatingTextActor);
    if (!FloatingTextComponent)
    {
        FloatingTextActor->Destroy();
        return nullptr;
    }

    FloatingTextActor->AddOwnedComponent(FloatingTextComponent);
    FloatingTextComponent->AttachToComponent(FloatingTextActor->GetRootComponent(),
        FAttachmentTransformRules::KeepWorldTransform);
    FloatingTextComponent->RegisterComponent();

    // Start the floating animation
    FloatingTextComponent->StartFloating(TextInfo);

    // Schedule destruction after the duration
    FloatingTextActor->SetLifeSpan(TextInfo.Duration + 0.5f);

    return FloatingTextComponent;
}

void UWowFloatingText::StartFloating(const FWowFloatingTextInfo& TextInfo)
{
    if (!TextRenderComponent)
    {
        return;
    }

    bIsFloating = true;
    FloatTimer = 0.0f;
    FloatDuration = TextInfo.Duration;
    FloatSpeed = TextInfo.Speed;
    StartLocation = GetComponentLocation();
    BaseColor = TextInfo.Color;

    // Setup text appearance
    TextRenderComponent->SetText(FText::FromString(TextInfo.Text));
    TextRenderComponent->SetTextRenderColor(TextInfo.Color.ToFColor(true));
    TextRenderComponent->SetWorldSize(TextInfo.FontSize);

    // Enable ticking
    SetComponentTickEnabled(true);
}

void UWowFloatingText::UpdateFloating(float DeltaTime)
{
    FloatTimer += DeltaTime;

    if (FloatTimer >= FloatDuration)
    {
        bIsFloating = false;
        SetComponentTickEnabled(false);
        return;
    }

    // Move upward
    FVector CurrentLocation = StartLocation + FVector(0.0f, 0.0f, FloatSpeed * FloatTimer);
    SetWorldLocation(CurrentLocation);

    // Fade out over time
    float Alpha = 1.0f - (FloatTimer / FloatDuration);
    Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

    if (TextRenderComponent)
    {
        FLinearColor FadedColor = BaseColor;
        FadedColor.A = Alpha;
        TextRenderComponent->SetTextRenderColor(FadedColor.ToFColor(true));
    }
}