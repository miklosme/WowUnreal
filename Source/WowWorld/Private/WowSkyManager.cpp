#include "WowSkyManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowSky, Log, All);

AWowSkyManager::AWowSkyManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // update 10x/sec
}

void AWowSkyManager::BeginPlay()
{
	Super::BeginPlay();

	// Create sun directional light
	SunLight = NewObject<UDirectionalLightComponent>(this, TEXT("SunLight"));
	SunLight->RegisterComponent();
	SunLight->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	SunLight->SetIntensity(3.14f);
	SunLight->SetLightColor(FLinearColor(1.0f, 0.95f, 0.85f));
	SunLight->SetCastShadows(true);

	// Create moon directional light
	MoonLight = NewObject<UDirectionalLightComponent>(this, TEXT("MoonLight"));
	MoonLight->RegisterComponent();
	MoonLight->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	MoonLight->SetIntensity(0.1f);
	MoonLight->SetLightColor(FLinearColor(0.5f, 0.55f, 0.7f));
	MoonLight->SetCastShadows(false);
	MoonLight->SetVisibility(false);

	// Create sky atmosphere
	SkyAtmosphere = NewObject<USkyAtmosphereComponent>(this, TEXT("SkyAtmosphere"));
	SkyAtmosphere->RegisterComponent();
	SkyAtmosphere->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	// Create sky light for ambient
	SkyLight = NewObject<USkyLightComponent>(this, TEXT("SkyLight"));
	SkyLight->RegisterComponent();
	SkyLight->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	SkyLight->SetIntensity(1.0f);
	SkyLight->bRealTimeCapture = true;

	// Create height fog
	if (bFogEnabled)
	{
		HeightFog = NewObject<UExponentialHeightFogComponent>(this, TEXT("HeightFog"));
		HeightFog->RegisterComponent();
		HeightFog->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		HeightFog->SetFogDensity(FogDensity);
		HeightFog->SetFogMaxOpacity(0.85f);
		HeightFog->SetFogHeightFalloff(0.001f);
		HeightFog->SetStartDistance(50000.0f); // 500m
	}

	// Initial update
	UpdateSunPosition();
	UpdateLightColors();
	UpdateFog();

	UE_LOG(LogWowSky, Log, TEXT("Sky manager initialized: time=%.0f, speed=%.1f"), TimeOfDay, TimeSpeed);
}

void AWowSkyManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Advance time
	if (TimeSpeed != 0.0f)
	{
		TimeOfDay += TimeSpeed * DeltaTime;
		while (TimeOfDay >= 1440.0f) TimeOfDay -= 1440.0f;
		while (TimeOfDay < 0.0f) TimeOfDay += 1440.0f;
	}

	UpdateSunPosition();
	UpdateLightColors();
	UpdateFog();
}

void AWowSkyManager::UpdateSunPosition()
{
	if (!SunLight) return;

	// Time 0=midnight, 360=6am, 720=noon, 1080=6pm, 1440=midnight
	// Sun angle: at noon (720) sun is highest (-90 pitch), at midnight (0) sun is lowest (+90)
	float NormalizedTime = TimeOfDay / 1440.0f; // 0-1
	float SunAngle = (NormalizedTime * 360.0f) - 90.0f; // -90 at midnight, +90 at noon, +270 at midnight

	// Sun pitch: 0 at horizon, -90 at zenith
	// At noon (NormalizedTime=0.5), sun should be at highest point
	float SunPitch = -90.0f * FMath::Sin(NormalizedTime * PI); // peaks at 0.5

	// Sun yaw rotates through the day
	float SunYaw = (NormalizedTime * 360.0f) + 180.0f;

	FRotator SunRotation(SunPitch, SunYaw, 0.0f);
	SunLight->SetWorldRotation(SunRotation);

	// Sun visibility: visible when pitch < 0 (above horizon)
	bool bSunAboveHorizon = SunPitch < -5.0f;
	SunLight->SetVisibility(bSunAboveHorizon);

	// Moon: opposite the sun
	if (MoonLight)
	{
		FRotator MoonRotation(SunPitch + 180.0f, SunYaw + 180.0f, 0.0f);
		MoonLight->SetWorldRotation(MoonRotation);
		MoonLight->SetVisibility(!bSunAboveHorizon);
	}
}

void AWowSkyManager::UpdateLightColors()
{
	if (!SunLight) return;

	FLinearColor SunColor = GetSunColorForTime();
	float SunIntensity = GetSunIntensityForTime();

	SunLight->SetLightColor(SunColor);
	SunLight->SetIntensity(SunIntensity);

	// Update sky light intensity based on time
	if (SkyLight)
	{
		float AmbientIntensity = FMath::Lerp(0.15f, 1.0f, SunIntensity / 3.14f);
		SkyLight->SetIntensity(AmbientIntensity);
	}
}

void AWowSkyManager::UpdateFog()
{
	if (!HeightFog) return;

	FLinearColor FogColor = GetFogColorForTime();
	HeightFog->SetFogInscatteringColor(FogColor);
}

FLinearColor AWowSkyManager::GetSunColorForTime() const
{
	// Dawn (5:00-7:00), Day (7:00-17:00), Dusk (17:00-19:00), Night (19:00-5:00)
	float T = TimeOfDay;

	if (T >= 300.0f && T < 420.0f) // Dawn: 5:00-7:00
	{
		float A = (T - 300.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(0.9f, 0.4f, 0.2f), FLinearColor(1.0f, 0.95f, 0.85f), A);
	}
	else if (T >= 420.0f && T < 1020.0f) // Day: 7:00-17:00
	{
		return FLinearColor(1.0f, 0.95f, 0.85f);
	}
	else if (T >= 1020.0f && T < 1140.0f) // Dusk: 17:00-19:00
	{
		float A = (T - 1020.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(1.0f, 0.95f, 0.85f), FLinearColor(0.9f, 0.4f, 0.2f), A);
	}
	else // Night
	{
		return FLinearColor(0.3f, 0.35f, 0.5f);
	}
}

float AWowSkyManager::GetSunIntensityForTime() const
{
	float T = TimeOfDay;

	if (T >= 300.0f && T < 420.0f) // Dawn
	{
		float A = (T - 300.0f) / 120.0f;
		return FMath::Lerp(0.1f, 3.14f, A);
	}
	else if (T >= 420.0f && T < 1020.0f) // Day
	{
		return 3.14f;
	}
	else if (T >= 1020.0f && T < 1140.0f) // Dusk
	{
		float A = (T - 1020.0f) / 120.0f;
		return FMath::Lerp(3.14f, 0.1f, A);
	}
	else // Night
	{
		return 0.1f;
	}
}

FLinearColor AWowSkyManager::GetFogColorForTime() const
{
	float T = TimeOfDay;

	if (T >= 300.0f && T < 420.0f) // Dawn
	{
		float A = (T - 300.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(0.05f, 0.05f, 0.1f), FLinearColor(0.7f, 0.75f, 0.85f), A);
	}
	else if (T >= 420.0f && T < 1020.0f) // Day
	{
		return FLinearColor(0.7f, 0.75f, 0.85f);
	}
	else if (T >= 1020.0f && T < 1140.0f) // Dusk
	{
		float A = (T - 1020.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(0.7f, 0.75f, 0.85f), FLinearColor(0.05f, 0.05f, 0.1f), A);
	}
	else // Night
	{
		return FLinearColor(0.05f, 0.05f, 0.1f);
	}
}

FLinearColor AWowSkyManager::GetSkyColorForTime() const
{
	float T = TimeOfDay;

	if (T >= 420.0f && T < 1020.0f) // Day
	{
		return FLinearColor(0.4f, 0.6f, 1.0f);
	}
	else if (T >= 300.0f && T < 420.0f) // Dawn
	{
		float A = (T - 300.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(0.1f, 0.1f, 0.2f), FLinearColor(0.4f, 0.6f, 1.0f), A);
	}
	else if (T >= 1020.0f && T < 1140.0f) // Dusk
	{
		float A = (T - 1020.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(0.4f, 0.6f, 1.0f), FLinearColor(0.1f, 0.1f, 0.2f), A);
	}
	else // Night
	{
		return FLinearColor(0.1f, 0.1f, 0.2f);
	}
}
