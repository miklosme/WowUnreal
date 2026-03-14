#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WowSkyManager.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;

/**
 * Manages time-of-day sky rendering with sun/moon positioning,
 * sky colors, and fog driven by Light.dbc data.
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

	/** Fog density */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WoW|Sky")
	float FogDensity = 0.002f;

	float GetTimeOfDay() const { return TimeOfDay; }

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

	/** Update sun/moon position based on time */
	void UpdateSunPosition();

	/** Update lighting colors based on time */
	void UpdateLightColors();

	/** Update fog based on time */
	void UpdateFog();

	/** Interpolate between day/night color profiles */
	FLinearColor GetSkyColorForTime() const;
	FLinearColor GetFogColorForTime() const;
	FLinearColor GetSunColorForTime() const;
	float GetSunIntensityForTime() const;
};
