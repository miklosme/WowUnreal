#pragma once
#include "CoreMinimal.h"

/**
 * Creates water, lava, and slime materials for liquid rendering.
 * Water: translucent with depth-based opacity and animated surface.
 * Lava: opaque with emissive glow and UV distortion.
 * Slime: opaque with slight emissive green.
 */
class WOWWORLD_API FWowWaterMaterial
{
public:
	/** Get or create the translucent water material (supports vertex color depth) */
	static UMaterial* GetWaterMaterial();

	/** Get or create the emissive lava material */
	static UMaterial* GetLavaMaterial();

	/** Get or create the emissive slime material */
	static UMaterial* GetSlimeMaterial();

	/** Get material for a given liquid category (0=water, 1=ocean, 2=magma, 3=slime) */
	static UMaterial* GetMaterialForCategory(int32 Category);
};
