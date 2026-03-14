#include "WowWaterMaterial.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#if WITH_EDITOR
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionClamp.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWowWaterMat, Log, All);

// ── Water Material ─────────────────────────────────────────────────────────────

UMaterial* FWowWaterMaterial::GetWaterMaterial()
{
	static UMaterial* CachedMat = nullptr;
	if (CachedMat && CachedMat->IsValidLowLevel()) return CachedMat;

#if WITH_EDITOR
	CachedMat = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_WowWater"));
	CachedMat->SetShadingModel(MSM_DefaultLit);
	// Use Masked blend mode instead of Translucent to avoid Metal translucency crash
	CachedMat->BlendMode = BLEND_Masked;
	CachedMat->TwoSided = true;
	CachedMat->OpacityMaskClipValue = 0.1f; // low threshold — mostly opaque with dithered edges

	auto& Exprs = CachedMat->GetExpressionCollection();

	// --- Vertex color: RGB carries tint, Alpha carries depth for opacity mask ---
	auto* VertColor = NewObject<UMaterialExpressionVertexColor>(CachedMat);
	VertColor->MaterialExpressionEditorX = -600;
	VertColor->MaterialExpressionEditorY = 0;
	Exprs.AddExpression(VertColor);

	// Extract RGB from vertex color for base color tint
	auto* RGBMask = NewObject<UMaterialExpressionComponentMask>(CachedMat);
	RGBMask->R = true; RGBMask->G = true; RGBMask->B = true; RGBMask->A = false;
	RGBMask->MaterialExpressionEditorX = -400;
	RGBMask->MaterialExpressionEditorY = 0;
	RGBMask->Input.Connect(0, VertColor);
	Exprs.AddExpression(RGBMask);

	// Extract alpha for opacity mask (depth-based)
	auto* AlphaMask = NewObject<UMaterialExpressionComponentMask>(CachedMat);
	AlphaMask->R = false; AlphaMask->G = false; AlphaMask->B = false; AlphaMask->A = true;
	AlphaMask->MaterialExpressionEditorX = -400;
	AlphaMask->MaterialExpressionEditorY = 400;
	AlphaMask->Input.Connect(0, VertColor);
	Exprs.AddExpression(AlphaMask);

	// --- Animated noise for surface variation ---
	auto* TexCoord = NewObject<UMaterialExpressionTextureCoordinate>(CachedMat);
	TexCoord->CoordinateIndex = 0;
	TexCoord->UTiling = 4.0f;
	TexCoord->VTiling = 4.0f;
	TexCoord->MaterialExpressionEditorX = -800;
	TexCoord->MaterialExpressionEditorY = 200;
	Exprs.AddExpression(TexCoord);

	auto* Panner = NewObject<UMaterialExpressionPanner>(CachedMat);
	Panner->SpeedX = 0.03f;
	Panner->SpeedY = 0.02f;
	Panner->MaterialExpressionEditorX = -600;
	Panner->MaterialExpressionEditorY = 200;
	Panner->Coordinate.Connect(0, TexCoord);
	Exprs.AddExpression(Panner);

	// Noise sampled with panning UVs for animated ripples
	auto* Noise = NewObject<UMaterialExpressionNoise>(CachedMat);
	Noise->Scale = 20.0f;
	Noise->Quality = 1;
	Noise->NoiseFunction = NOISEFUNCTION_GradientALU;
	Noise->bTurbulence = true;
	Noise->Levels = 3;
	Noise->OutputMin = -0.05f;
	Noise->OutputMax = 0.05f;
	Noise->MaterialExpressionEditorX = -400;
	Noise->MaterialExpressionEditorY = 200;
	Noise->Position.Connect(0, Panner);
	Exprs.AddExpression(Noise);

	// Add noise to vertex color RGB for animated surface variation
	auto* ColorAdd = NewObject<UMaterialExpressionAdd>(CachedMat);
	ColorAdd->MaterialExpressionEditorX = -200;
	ColorAdd->MaterialExpressionEditorY = 0;
	ColorAdd->A.Connect(0, RGBMask);
	ColorAdd->B.Connect(0, Noise);
	Exprs.AddExpression(ColorAdd);

	// --- Specular/Roughness for water reflections ---
	auto* Specular = NewObject<UMaterialExpressionConstant>(CachedMat);
	Specular->R = 0.8f;
	Specular->MaterialExpressionEditorX = -200;
	Specular->MaterialExpressionEditorY = 300;
	Exprs.AddExpression(Specular);

	auto* Roughness = NewObject<UMaterialExpressionConstant>(CachedMat);
	Roughness->R = 0.15f;
	Roughness->MaterialExpressionEditorX = -200;
	Roughness->MaterialExpressionEditorY = 350;
	Exprs.AddExpression(Roughness);

	// --- Connect outputs ---
	CachedMat->GetEditorOnlyData()->BaseColor.Connect(0, ColorAdd);
	CachedMat->GetEditorOnlyData()->OpacityMask.Connect(0, AlphaMask);
	CachedMat->GetEditorOnlyData()->Specular.Connect(0, Specular);
	CachedMat->GetEditorOnlyData()->Roughness.Connect(0, Roughness);

	CachedMat->PreEditChange(nullptr);
	CachedMat->PostEditChange();

	UE_LOG(LogWowWaterMat, Log, TEXT("Water material created (masked + animated noise + depth opacity)"));
#else
	CachedMat = UMaterial::GetDefaultMaterial(MD_Surface);
	UE_LOG(LogWowWaterMat, Warning, TEXT("Non-editor: using default surface material for water"));
#endif

	return CachedMat;
}

// ── Lava Material ──────────────────────────────────────────────────────────────

UMaterial* FWowWaterMaterial::GetLavaMaterial()
{
	static UMaterial* CachedMat = nullptr;
	if (CachedMat && CachedMat->IsValidLowLevel()) return CachedMat;

#if WITH_EDITOR
	CachedMat = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_WowLava"));
	CachedMat->SetShadingModel(MSM_DefaultLit);
	CachedMat->BlendMode = BLEND_Opaque;
	CachedMat->TwoSided = false;

	auto& Exprs = CachedMat->GetExpressionCollection();

	// --- Dark base color ---
	auto* BaseColor = NewObject<UMaterialExpressionConstant3Vector>(CachedMat);
	BaseColor->Constant = FLinearColor(0.15f, 0.02f, 0.0f);
	BaseColor->MaterialExpressionEditorX = -400;
	BaseColor->MaterialExpressionEditorY = 0;
	Exprs.AddExpression(BaseColor);

	// --- Emissive: animated orange/red glow ---
	auto* EmissiveColor = NewObject<UMaterialExpressionConstant3Vector>(CachedMat);
	EmissiveColor->Constant = FLinearColor(4.0f, 1.2f, 0.15f); // HDR emissive for bloom
	EmissiveColor->MaterialExpressionEditorX = -600;
	EmissiveColor->MaterialExpressionEditorY = 200;
	Exprs.AddExpression(EmissiveColor);

	// Animated UV for lava flow
	auto* TexCoord = NewObject<UMaterialExpressionTextureCoordinate>(CachedMat);
	TexCoord->CoordinateIndex = 0;
	TexCoord->UTiling = 3.0f;
	TexCoord->VTiling = 3.0f;
	TexCoord->MaterialExpressionEditorX = -800;
	TexCoord->MaterialExpressionEditorY = 300;
	Exprs.AddExpression(TexCoord);

	auto* Panner = NewObject<UMaterialExpressionPanner>(CachedMat);
	Panner->SpeedX = 0.015f;
	Panner->SpeedY = 0.01f;
	Panner->MaterialExpressionEditorX = -600;
	Panner->MaterialExpressionEditorY = 300;
	Panner->Coordinate.Connect(0, TexCoord);
	Exprs.AddExpression(Panner);

	// Noise for lava surface variation
	auto* Noise = NewObject<UMaterialExpressionNoise>(CachedMat);
	Noise->Scale = 10.0f;
	Noise->Quality = 1;
	Noise->NoiseFunction = NOISEFUNCTION_GradientALU;
	Noise->bTurbulence = true;
	Noise->Levels = 4;
	Noise->OutputMin = 0.3f;
	Noise->OutputMax = 1.0f;
	Noise->MaterialExpressionEditorX = -400;
	Noise->MaterialExpressionEditorY = 300;
	Noise->Position.Connect(0, Panner);
	Exprs.AddExpression(Noise);

	// Multiply emissive by noise for animated glow pattern
	auto* EmissiveMul = NewObject<UMaterialExpressionMultiply>(CachedMat);
	EmissiveMul->MaterialExpressionEditorX = -200;
	EmissiveMul->MaterialExpressionEditorY = 200;
	EmissiveMul->A.Connect(0, EmissiveColor);
	EmissiveMul->B.Connect(0, Noise);
	Exprs.AddExpression(EmissiveMul);

	auto* Roughness = NewObject<UMaterialExpressionConstant>(CachedMat);
	Roughness->R = 0.6f;
	Roughness->MaterialExpressionEditorX = -200;
	Roughness->MaterialExpressionEditorY = 400;
	Exprs.AddExpression(Roughness);

	CachedMat->GetEditorOnlyData()->BaseColor.Connect(0, BaseColor);
	CachedMat->GetEditorOnlyData()->EmissiveColor.Connect(0, EmissiveMul);
	CachedMat->GetEditorOnlyData()->Roughness.Connect(0, Roughness);

	CachedMat->PreEditChange(nullptr);
	CachedMat->PostEditChange();

	UE_LOG(LogWowWaterMat, Log, TEXT("Lava material created (emissive + animated noise)"));
#else
	CachedMat = UMaterial::GetDefaultMaterial(MD_Surface);
	UE_LOG(LogWowWaterMat, Warning, TEXT("Non-editor: using default surface material for lava"));
#endif

	return CachedMat;
}

// ── Slime Material ─────────────────────────────────────────────────────────────

UMaterial* FWowWaterMaterial::GetSlimeMaterial()
{
	static UMaterial* CachedMat = nullptr;
	if (CachedMat && CachedMat->IsValidLowLevel()) return CachedMat;

#if WITH_EDITOR
	CachedMat = NewObject<UMaterial>(GetTransientPackage(), TEXT("M_WowSlime"));
	CachedMat->SetShadingModel(MSM_DefaultLit);
	CachedMat->BlendMode = BLEND_Opaque;
	CachedMat->TwoSided = false;

	auto& Exprs = CachedMat->GetExpressionCollection();

	// --- Green base color ---
	auto* BaseColor = NewObject<UMaterialExpressionConstant3Vector>(CachedMat);
	BaseColor->Constant = FLinearColor(0.08f, 0.25f, 0.05f);
	BaseColor->MaterialExpressionEditorX = -400;
	BaseColor->MaterialExpressionEditorY = 0;
	Exprs.AddExpression(BaseColor);

	// --- Slight emissive glow ---
	auto* EmissiveColor = NewObject<UMaterialExpressionConstant3Vector>(CachedMat);
	EmissiveColor->Constant = FLinearColor(0.3f, 1.0f, 0.1f); // Slight green glow
	EmissiveColor->MaterialExpressionEditorX = -600;
	EmissiveColor->MaterialExpressionEditorY = 200;
	Exprs.AddExpression(EmissiveColor);

	// Slow animated UV for slime
	auto* TexCoord = NewObject<UMaterialExpressionTextureCoordinate>(CachedMat);
	TexCoord->CoordinateIndex = 0;
	TexCoord->UTiling = 2.0f;
	TexCoord->VTiling = 2.0f;
	TexCoord->MaterialExpressionEditorX = -800;
	TexCoord->MaterialExpressionEditorY = 300;
	Exprs.AddExpression(TexCoord);

	auto* Panner = NewObject<UMaterialExpressionPanner>(CachedMat);
	Panner->SpeedX = 0.008f;
	Panner->SpeedY = 0.005f;
	Panner->MaterialExpressionEditorX = -600;
	Panner->MaterialExpressionEditorY = 300;
	Panner->Coordinate.Connect(0, TexCoord);
	Exprs.AddExpression(Panner);

	auto* Noise = NewObject<UMaterialExpressionNoise>(CachedMat);
	Noise->Scale = 8.0f;
	Noise->Quality = 1;
	Noise->NoiseFunction = NOISEFUNCTION_GradientALU;
	Noise->bTurbulence = true;
	Noise->Levels = 2;
	Noise->OutputMin = 0.4f;
	Noise->OutputMax = 1.0f;
	Noise->MaterialExpressionEditorX = -400;
	Noise->MaterialExpressionEditorY = 300;
	Noise->Position.Connect(0, Panner);
	Exprs.AddExpression(Noise);

	auto* EmissiveMul = NewObject<UMaterialExpressionMultiply>(CachedMat);
	EmissiveMul->MaterialExpressionEditorX = -200;
	EmissiveMul->MaterialExpressionEditorY = 200;
	EmissiveMul->A.Connect(0, EmissiveColor);
	EmissiveMul->B.Connect(0, Noise);
	Exprs.AddExpression(EmissiveMul);

	auto* Roughness = NewObject<UMaterialExpressionConstant>(CachedMat);
	Roughness->R = 0.4f;
	Roughness->MaterialExpressionEditorX = -200;
	Roughness->MaterialExpressionEditorY = 400;
	Exprs.AddExpression(Roughness);

	CachedMat->GetEditorOnlyData()->BaseColor.Connect(0, BaseColor);
	CachedMat->GetEditorOnlyData()->EmissiveColor.Connect(0, EmissiveMul);
	CachedMat->GetEditorOnlyData()->Roughness.Connect(0, Roughness);

	CachedMat->PreEditChange(nullptr);
	CachedMat->PostEditChange();

	UE_LOG(LogWowWaterMat, Log, TEXT("Slime material created (emissive green + animated noise)"));
#else
	CachedMat = UMaterial::GetDefaultMaterial(MD_Surface);
	UE_LOG(LogWowWaterMat, Warning, TEXT("Non-editor: using default surface material for slime"));
#endif

	return CachedMat;
}

// ── Category Lookup ────────────────────────────────────────────────────────────

UMaterial* FWowWaterMaterial::GetMaterialForCategory(int32 Category)
{
	switch (Category)
	{
	case 0: return GetWaterMaterial();  // water
	case 1: return GetWaterMaterial();  // ocean (same material as water)
	case 2: return GetLavaMaterial();   // magma
	case 3: return GetSlimeMaterial();  // slime
	default: return GetWaterMaterial();
	}
}
