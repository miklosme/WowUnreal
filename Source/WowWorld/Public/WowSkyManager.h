#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WowSkyManager.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
struct FLightDbcEntry;

/**
 * Manages time-of-day sky rendering with sun/moon positioning,
 * sky gradient colors, fog, and clouds driven by Light.dbc data with zone blending.
 */
UCLASS()
class WOWWORLD_API AWowSkyManager : public AActor
{
	GENERATED_BODY()
public:
	AWowSkyManager();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Current time in minutes (0-1440, where 720 = noon) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Sky")
	float TimeOfDay = 600.0f; // 10:00 AM

	/** Speed: game minutes per real second. 0 = frozen. 1 = realtime. 60 = 24 min cycle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Sky")
	float TimeSpeed = 1.0f;

	/** Enable fog */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Sky")
	bool bFogEnabled = true;

	/** Fog density (fallback when LightFloatParams not loaded) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Sky")
	float FogDensity = 0.002f;

	/** Current map ID for Light.dbc lookups */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Sky")
	int32 CurrentMapId = 0;

	float GetTimeOfDay() const { return TimeOfDay; }

	/** Load light zones from Light.dbc for the given map */
	void LoadLightZones(int32 MapId);

private:
	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> SunLight;

	UPROPERTY()
	TObjectPtr<UDirectionalLightComponent> MoonLight;

	UPROPERTY()
	TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;

	UPROPERTY()
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY()
	TObjectPtr<UExponentialHeightFogComponent> HeightFog;

	/** Skydome mesh for gradient band rendering */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SkydomeMesh;

	/** Cloud plane mesh */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> CloudMesh;

	/** Dynamic material for skydome gradient */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> SkydomeMaterial;

	/** Dynamic material for cloud layer */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CloudMaterial;

	/** Cached light zone entries for the current map */
	TArray<const FLightDbcEntry*> MapLights;

	/** Whether we have DBC-driven light data loaded */
	bool bHasDbcLights = false;

	/** Whether LightFloatParams loaded */
	bool bHasFloatParams = false;

	// ── Light property indices in LightIntParams ──
	// Each Light.dbc paramID maps to 18 sequential LightIntParams rows
	enum ELightProperty : uint8
	{
		LP_GlobalAmbient = 0,
		LP_GlobalDiffuse = 1,
		LP_SkyTopColor = 2,
		LP_SkyMiddleColor = 3,
		LP_SkyBand1 = 4,
		LP_SkyBand2 = 5,
		LP_SkySmogColor = 6,
		LP_SkyFogColor = 7,
		LP_SunColor = 8,
		LP_SunHaloColor = 9,
		LP_MAX = 18
	};

	// Float param property indices (LightFloatParams rows per paramID)
	enum ELightFloatProperty : uint8
	{
		FP_FogDistance = 0,
		FP_FogMultiplier = 1,
		FP_CelestialGlowThrough = 2,
		FP_CloudDensity = 3,
		FP_MAX = 6
	};

	/** Interpolate a LightIntParams color band at the current time */
	FLinearColor InterpolateDbcColor(uint32 ParamID, ELightProperty Property) const;

	/** Interpolate a LightFloatParams value at the current time */
	float InterpolateDbcFloat(uint32 ParamID, ELightFloatProperty Property) const;

	/** Blend colors from all nearby light zones weighted by distance */
	FLinearColor BlendZoneColor(ELightProperty Property, const FVector& PlayerPos) const;

	/** Blend float params from all nearby light zones */
	float BlendZoneFloat(ELightFloatProperty Property, const FVector& PlayerPos) const;

	/** Update sun/moon position based on time */
	void UpdateSunPosition();

	/** Update lighting colors based on time */
	void UpdateLightColors();

	/** Update fog based on time */
	void UpdateFog();

	/** Update skydome gradient colors */
	void UpdateSkydome();

	/** Update cloud layer */
	void UpdateClouds();

	/** Create the skydome hemisphere mesh */
	void CreateSkydomeMesh();

	/** Create the cloud plane mesh */
	void CreateCloudMesh();

	/** Fallback interpolation (used when DBC data not available) */
	FLinearColor GetSkyColorFallback() const;
	FLinearColor GetFogColorFallback() const;
	FLinearColor GetSunColorFallback() const;
	float GetSunIntensityForTime() const;
};
