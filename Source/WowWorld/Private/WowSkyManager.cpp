#include "WowSkyManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Formats/Dbc/DbcStore.h"
#include "Formats/Dbc/LightDbc.h"
#include "Formats/Dbc/LightParamsDbc.h"
#include "Formats/Dbc/LightIntParamsDbc.h"
#include "Coord/WowCoordinate.h"
#include "Kismet/GameplayStatics.h"

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

	// Load light zones from DBC
	LoadLightZones(CurrentMapId);

	// Initial update
	UpdateSunPosition();
	UpdateLightColors();
	UpdateFog();

	UE_LOG(LogWowSky, Log, TEXT("Sky manager initialized: time=%.0f, speed=%.1f, dbcLights=%d"),
		TimeOfDay, TimeSpeed, MapLights.Num());
}

void AWowSkyManager::LoadLightZones(int32 MapId)
{
	CurrentMapId = MapId;
	MapLights.Empty();

	const FDbcStore& Dbc = FDbcStore::Get();
	if (Dbc.Lights().Num() == 0)
	{
		UE_LOG(LogWowSky, Log, TEXT("Light.dbc not loaded, using fallback colors"));
		bHasDbcLights = false;
		return;
	}

	MapLights = Dbc.Lights().GetByMap(static_cast<uint32>(MapId));
	bHasDbcLights = MapLights.Num() > 0;

	UE_LOG(LogWowSky, Log, TEXT("Loaded %d light zones for map %d"), MapLights.Num(), MapId);
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
	float NormalizedTime = TimeOfDay / 1440.0f; // 0-1

	// Sun pitch: peaks at noon (NormalizedTime=0.5)
	float SunPitch = -90.0f * FMath::Sin(NormalizedTime * PI);

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

	FLinearColor SunColor;
	float SunIntensity = GetSunIntensityForTime();

	if (bHasDbcLights)
	{
		FVector PlayerPos = FVector::ZeroVector;
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			PlayerPos = Pawn->GetActorLocation();
		}

		SunColor = BlendZoneColor(LP_SunColor, PlayerPos);
		FLinearColor AmbientColor = BlendZoneColor(LP_GlobalAmbient, PlayerPos);

		SunLight->SetLightColor(SunColor);
		SunLight->SetIntensity(SunIntensity);

		if (SkyLight)
		{
			SkyLight->SetLightColor(AmbientColor);
			float AmbientIntensity = FMath::Lerp(0.15f, 1.0f, SunIntensity / 3.14f);
			SkyLight->SetIntensity(AmbientIntensity);
		}
	}
	else
	{
		SunColor = GetSunColorFallback();
		SunLight->SetLightColor(SunColor);
		SunLight->SetIntensity(SunIntensity);

		if (SkyLight)
		{
			float AmbientIntensity = FMath::Lerp(0.15f, 1.0f, SunIntensity / 3.14f);
			SkyLight->SetIntensity(AmbientIntensity);
		}
	}
}

void AWowSkyManager::UpdateFog()
{
	if (!HeightFog) return;

	FLinearColor FogColor;

	if (bHasDbcLights)
	{
		FVector PlayerPos = FVector::ZeroVector;
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			PlayerPos = Pawn->GetActorLocation();
		}
		FogColor = BlendZoneColor(LP_SkyFogColor, PlayerPos);
	}
	else
	{
		FogColor = GetFogColorFallback();
	}

	HeightFog->SetFogInscatteringColor(FogColor);
}

// ── DBC Color Interpolation ────────────────────────────────────────────────────

FLinearColor AWowSkyManager::InterpolateDbcColor(uint32 ParamID, ELightProperty Property) const
{
	if (ParamID == 0) return FLinearColor::White;

	const FDbcStore& Dbc = FDbcStore::Get();

	// LightIntParams ID = (ParamID - 1) * 18 + PropertyIndex + 1
	uint32 IntParamID = (ParamID - 1) * 18 + static_cast<uint32>(Property) + 1;
	const FLightIntParamsDbcEntry* Entry = Dbc.LightIntParams().GetById(IntParamID);
	if (!Entry || Entry->EntryCount == 0)
	{
		return FLinearColor::White;
	}

	// Time in DBC is stored as half-minutes (0-2880)
	uint32 DbcTime = static_cast<uint32>(TimeOfDay * 2.0f);

	// Find the two bracketing time entries and interpolate
	uint32 Count = FMath::Min(Entry->EntryCount, static_cast<uint32>(FLightIntParamsDbcEntry::MaxEntries));

	// Find lower and upper bounds
	uint32 LowIdx = Count - 1;
	uint32 HighIdx = 0;
	for (uint32 i = 0; i < Count; i++)
	{
		if (Entry->Times[i] <= DbcTime)
		{
			LowIdx = i;
		}
		if (Entry->Times[i] > DbcTime && HighIdx == 0)
		{
			HighIdx = i;
		}
	}
	if (HighIdx == 0) HighIdx = 0; // wrap around

	// Decode BGRA packed color values
	auto DecodeColor = [](uint32 Packed) -> FLinearColor
	{
		float B = ((Packed >> 0) & 0xFF) / 255.0f;
		float G = ((Packed >> 8) & 0xFF) / 255.0f;
		float R = ((Packed >> 16) & 0xFF) / 255.0f;
		return FLinearColor(R, G, B);
	};

	FLinearColor LowColor = DecodeColor(Entry->Values[LowIdx]);
	FLinearColor HighColor = DecodeColor(Entry->Values[HighIdx]);

	// Interpolation factor
	uint32 LowTime = Entry->Times[LowIdx];
	uint32 HighTime = Entry->Times[HighIdx];

	if (HighTime <= LowTime)
	{
		// Wrapping: high is next day
		HighTime += 2880;
		uint32 AdjTime = (DbcTime < LowTime) ? DbcTime + 2880 : DbcTime;
		float Alpha = (HighTime > LowTime) ? static_cast<float>(AdjTime - LowTime) / (HighTime - LowTime) : 0.0f;
		return FMath::Lerp(LowColor, HighColor, FMath::Clamp(Alpha, 0.0f, 1.0f));
	}

	float Alpha = static_cast<float>(DbcTime - LowTime) / FMath::Max(1u, HighTime - LowTime);
	return FMath::Lerp(LowColor, HighColor, FMath::Clamp(Alpha, 0.0f, 1.0f));
}

FLinearColor AWowSkyManager::BlendZoneColor(ELightProperty Property, const FVector& PlayerPos) const
{
	if (MapLights.Num() == 0) return FLinearColor::White;

	// Convert player UE position back to WoW coordinates for distance checks
	FVector WowPos = FWowCoordinate::UEToWow(PlayerPos);

	FLinearColor Result = FLinearColor::Black;
	float TotalWeight = 0.0f;

	// First entry with FalloffEnd == 0 is the global/default light for this map
	const FLightDbcEntry* DefaultLight = nullptr;

	for (const FLightDbcEntry* Light : MapLights)
	{
		if (Light->FalloffEnd == 0.0f)
		{
			DefaultLight = Light;
			continue;
		}

		// Distance from player to light center
		FVector LightPos(Light->X, Light->Y, Light->Z);
		float Dist = FVector::Dist(WowPos, LightPos);

		if (Dist > Light->FalloffEnd) continue;

		// Weight: 1.0 inside FalloffStart, lerp to 0.0 at FalloffEnd
		float Weight = 1.0f;
		if (Dist > Light->FalloffStart && Light->FalloffEnd > Light->FalloffStart)
		{
			Weight = 1.0f - (Dist - Light->FalloffStart) / (Light->FalloffEnd - Light->FalloffStart);
		}

		// Use the first non-zero paramID (index 0 is for normal weather)
		uint32 ParamID = Light->ParamIDs[0];
		if (ParamID == 0) continue;

		FLinearColor ZoneColor = InterpolateDbcColor(ParamID, Property);
		Result += ZoneColor * Weight;
		TotalWeight += Weight;
	}

	if (TotalWeight < 1.0f && DefaultLight)
	{
		// Fill remainder with default light
		uint32 DefaultParamID = DefaultLight->ParamIDs[0];
		if (DefaultParamID != 0)
		{
			FLinearColor DefaultColor = InterpolateDbcColor(DefaultParamID, Property);
			float DefaultWeight = 1.0f - TotalWeight;
			Result += DefaultColor * DefaultWeight;
			TotalWeight += DefaultWeight;
		}
	}

	if (TotalWeight > 0.0f)
	{
		Result /= TotalWeight;
	}
	else
	{
		// No DBC lights matched — fall back to hardcoded
		switch (Property)
		{
		case LP_SunColor: return GetSunColorFallback();
		case LP_SkyFogColor: return GetFogColorFallback();
		default: return GetSkyColorFallback();
		}
	}

	return Result;
}

// ── Fallback (hardcoded) color functions ────────────────────────────────────────

FLinearColor AWowSkyManager::GetSunColorFallback() const
{
	float T = TimeOfDay;
	if (T >= 300.0f && T < 420.0f) // Dawn
	{
		float A = (T - 300.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(0.9f, 0.4f, 0.2f), FLinearColor(1.0f, 0.95f, 0.85f), A);
	}
	else if (T >= 420.0f && T < 1020.0f) // Day
	{
		return FLinearColor(1.0f, 0.95f, 0.85f);
	}
	else if (T >= 1020.0f && T < 1140.0f) // Dusk
	{
		float A = (T - 1020.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(1.0f, 0.95f, 0.85f), FLinearColor(0.9f, 0.4f, 0.2f), A);
	}
	return FLinearColor(0.3f, 0.35f, 0.5f); // Night
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
	return 0.1f; // Night
}

FLinearColor AWowSkyManager::GetFogColorFallback() const
{
	float T = TimeOfDay;
	if (T >= 300.0f && T < 420.0f)
	{
		float A = (T - 300.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(0.05f, 0.05f, 0.1f), FLinearColor(0.7f, 0.75f, 0.85f), A);
	}
	else if (T >= 420.0f && T < 1020.0f)
	{
		return FLinearColor(0.7f, 0.75f, 0.85f);
	}
	else if (T >= 1020.0f && T < 1140.0f)
	{
		float A = (T - 1020.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(0.7f, 0.75f, 0.85f), FLinearColor(0.05f, 0.05f, 0.1f), A);
	}
	return FLinearColor(0.05f, 0.05f, 0.1f); // Night
}

FLinearColor AWowSkyManager::GetSkyColorFallback() const
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
	return FLinearColor(0.1f, 0.1f, 0.2f); // Night
}
