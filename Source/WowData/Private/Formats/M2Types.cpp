#include "Formats/M2Types.h"

bool FM2ParticleEmitter::IsFireEmitter() const
{
    // Fire emitters typically have warm colors (red/orange/yellow) and high emission rates
    // Check for red/orange/yellow dominance in start color
    float RedComponent = ColorStart[0] / 255.0f;
    float GreenComponent = ColorStart[1] / 255.0f;
    float BlueComponent = ColorStart[2] / 255.0f;

    // Fire is typically red-orange: high red, moderate green, low blue
    bool IsWarmColor = (RedComponent > 0.6f && GreenComponent > 0.2f && BlueComponent < 0.5f);

    // Fire emitters also typically have higher emission rates and shorter lifespans
    bool IsFireLike = (EmissionRate > 5.0f && HeadLifeStart < 3000.0f);

    return IsWarmColor || IsFireLike;
}

bool FM2ParticleEmitter::IsSmokeEmitter() const
{
    // Smoke emitters typically have grey colors and rise upward (negative gravity or upward motion)
    // Check for greyish colors
    float RedComponent = ColorStart[0] / 255.0f;
    float GreenComponent = ColorStart[1] / 255.0f;
    float BlueComponent = ColorStart[2] / 255.0f;

    // Grey colors have similar RGB values
    float ColorVariance = FMath::Abs(RedComponent - GreenComponent) +
                         FMath::Abs(GreenComponent - BlueComponent) +
                         FMath::Abs(BlueComponent - RedComponent);

    bool IsGreyish = (ColorVariance < 0.3f && RedComponent > 0.2f && RedComponent < 0.8f);

    // Smoke typically has longer lifespan and lower emission rate
    bool IsSmokelike = (HeadLifeStart > 2000.0f && EmissionRate < 15.0f);

    // Also check if gravity is positive (particles fall down, suggesting smoke rising up initially)
    bool HasUpwardMotion = (Gravity > -5.0f);

    return IsGreyish && (IsSmokelike || HasUpwardMotion);
}

bool FM2ParticleEmitter::IsSparkleEmitter() const
{
    // Sparkle emitters typically have bright colors, low emission rates, and small scales
    float RedComponent = ColorStart[0] / 255.0f;
    float GreenComponent = ColorStart[1] / 255.0f;
    float BlueComponent = ColorStart[2] / 255.0f;

    // Bright/saturated colors
    float MaxComponent = FMath::Max3(RedComponent, GreenComponent, BlueComponent);
    bool IsBright = (MaxComponent > 0.7f);

    // Low emission rate and small scale suggests ambient sparkles
    bool IsSparklelike = (EmissionRate < 8.0f && ScaleStart < 2.0f);

    // Sparkles often have shorter lifespans but not as short as fire
    bool IsAmbientTiming = (HeadLifeStart > 500.0f && HeadLifeStart < 2500.0f);

    return IsBright && IsSparklelike && IsAmbientTiming;
}