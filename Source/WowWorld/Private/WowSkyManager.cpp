#include "WowSkyManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Formats/Dbc/DbcStore.h"
#include "Formats/Dbc/LightDbc.h"
#include "Formats/Dbc/LightParamsDbc.h"
#include "Formats/Dbc/LightIntParamsDbc.h"
#include "Formats/Dbc/LightFloatParamsDbc.h"
#include "Coord/WowCoordinate.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"

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
		HeightFog->SetStartDistance(50000.0f);
	}

	// Create skydome for gradient band rendering
	CreateSkydomeMesh();

	// Create cloud plane
	CreateCloudMesh();

	// Create sun/moon disc billboards
	CreateCelestialDiscs();

	// Parse -timeofday= command-line override for deterministic verification
	float CmdTimeOfDay = -1.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("-timeofday="), CmdTimeOfDay))
	{
		TimeOfDay = FMath::Clamp(CmdTimeOfDay, 0.0f, 1439.9f);
		TimeSpeed = 0.0f; // Freeze time for screenshots
		UE_LOG(LogWowSky, Log, TEXT("Time-of-day override: %.0f minutes (frozen)"), TimeOfDay);
	}

	// Load light zones from DBC
	LoadLightZones(CurrentMapId);

	// Check if float params are available
	bHasFloatParams = FDbcStore::Get().LightFloatParams().Num() > 0;

	// Initial update
	UpdateSunPosition();
	UpdateLightColors();
	UpdateFog();
	UpdateSkydome();
	UpdateClouds();
	UpdateCelestialDiscs();

	UE_LOG(LogWowSky, Log, TEXT("Sky manager initialized: time=%.0f, speed=%.1f, dbcLights=%d, floatParams=%s"),
		TimeOfDay, TimeSpeed, MapLights.Num(), bHasFloatParams ? TEXT("yes") : TEXT("no"));
}

// ── Skydome Mesh Creation ────────────────────────────────────────────────────────

static UStaticMesh* BuildDomeMesh(
	UObject* Outer,
	const TArray<FVector>& Vertices,
	const TArray<int32>& Indices,
	const TArray<FVector4f>& Colors)
{
	FMeshDescription MeshDesc;
	FStaticMeshAttributes Attrs(MeshDesc);
	Attrs.Register();

	TVertexAttributesRef<FVector3f> Positions = Attrs.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> Normals = Attrs.GetVertexInstanceNormals();
	TVertexInstanceAttributesRef<FVector3f> Tangents = Attrs.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> BinormalSigns = Attrs.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector2f> UVs = Attrs.GetVertexInstanceUVs();
	TVertexInstanceAttributesRef<FVector4f> VColors = Attrs.GetVertexInstanceColors();

	FPolygonGroupID PolyGroup = MeshDesc.CreatePolygonGroup();
	MeshDesc.ReserveNewVertices(Vertices.Num());
	MeshDesc.ReserveNewVertexInstances(Vertices.Num());
	MeshDesc.ReserveNewPolygons(Indices.Num() / 3);

	TArray<FVertexInstanceID> InstanceIDs;
	InstanceIDs.SetNum(Vertices.Num());

	for (int32 v = 0; v < Vertices.Num(); ++v)
	{
		FVertexID VID = MeshDesc.CreateVertex();
		Positions[VID] = FVector3f(Vertices[v]);

		FVertexInstanceID IID = MeshDesc.CreateVertexInstance(VID);
		InstanceIDs[v] = IID;

		Normals[IID] = FVector3f(0, 0, -1);
		Tangents[IID] = FVector3f(1, 0, 0);
		BinormalSigns[IID] = 1.0f;
		UVs.Set(IID, 0, FVector2f(Colors[v].X, 0.0f)); // Store height in U
		VColors[IID] = Colors[v];
	}

	for (int32 t = 0; t < Indices.Num(); t += 3)
	{
		TArray<FVertexInstanceID> Tri;
		Tri.Add(InstanceIDs[Indices[t]]);
		Tri.Add(InstanceIDs[Indices[t + 1]]);
		Tri.Add(InstanceIDs[Indices[t + 2]]);
		MeshDesc.CreatePolygon(PolyGroup, Tri);
	}

	UStaticMesh* SM = NewObject<UStaticMesh>(Outer);
	SM->GetStaticMaterials().Add(FStaticMaterial());
	TArray<const FMeshDescription*> Descs = {&MeshDesc};
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bBuildSimpleCollision = false;
	Params.bFastBuild = true;
	SM->BuildFromMeshDescriptions(Descs, Params);
	return SM;
}

void AWowSkyManager::CreateSkydomeMesh()
{
	const int32 NumRings = 16;
	const int32 NumSegments = 32;
	const float Radius = 900000.0f; // 9km

	TArray<FVector> Verts;
	TArray<FVector4f> Colors;
	TArray<int32> Indices;

	// Generate vertices: rings from horizon (ring 0) to zenith (ring NumRings-1)
	for (int32 Ring = 0; Ring < NumRings; ++Ring)
	{
		float HeightFrac = static_cast<float>(Ring) / (NumRings - 1); // 0=horizon, 1=zenith
		float Phi = (PI / 2.0f) * HeightFrac;
		float Y = FMath::Sin(Phi) * Radius;
		float RingRadius = FMath::Cos(Phi) * Radius;

		for (int32 Seg = 0; Seg < NumSegments; ++Seg)
		{
			float Theta = 2.0f * PI * Seg / NumSegments;
			float X = FMath::Cos(Theta) * RingRadius;
			float Z = FMath::Sin(Theta) * RingRadius;
			Verts.Add(FVector(X, Z, Y));
			Colors.Add(FVector4f(HeightFrac, HeightFrac, HeightFrac, 1.0f));
		}
	}

	// Zenith cap vertex
	Verts.Add(FVector(0.0f, 0.0f, Radius));
	Colors.Add(FVector4f(1.0f, 1.0f, 1.0f, 1.0f));

	auto GetIdx = [&](int32 Ring, int32 Seg) -> int32
	{
		return Ring * NumSegments + (Seg % NumSegments);
	};

	// Triangles between rings (inverted winding for inside view)
	for (int32 Ring = 0; Ring < NumRings - 1; ++Ring)
	{
		for (int32 Seg = 0; Seg < NumSegments; ++Seg)
		{
			int32 I00 = GetIdx(Ring, Seg);
			int32 I10 = GetIdx(Ring, Seg + 1);
			int32 I01 = GetIdx(Ring + 1, Seg);
			int32 I11 = GetIdx(Ring + 1, Seg + 1);

			// Triangle 1 (inverted winding)
			Indices.Add(I00);
			Indices.Add(I01);
			Indices.Add(I10);

			// Triangle 2 (inverted winding)
			Indices.Add(I10);
			Indices.Add(I01);
			Indices.Add(I11);
		}
	}

	// Cap triangles connecting last ring to zenith
	int32 ZenithIdx = Verts.Num() - 1;
	int32 LastRing = NumRings - 1;
	for (int32 Seg = 0; Seg < NumSegments; ++Seg)
	{
		Indices.Add(GetIdx(LastRing, Seg));
		Indices.Add(ZenithIdx);
		Indices.Add(GetIdx(LastRing, Seg + 1));
	}

	UStaticMesh* Mesh = BuildDomeMesh(this, Verts, Indices, Colors);

	// Create skydome material: unlit, two-sided, vertex color R = height blend
	UMaterial* BaseMat = NewObject<UMaterial>(this, TEXT("SkydomeMat_Base"));
	BaseMat->SetShadingModel(MSM_Unlit);
	BaseMat->TwoSided = true;
	BaseMat->BlendMode = BLEND_Opaque;

	// Vector parameters for gradient band colors
	auto* PTopColor = NewObject<UMaterialExpressionVectorParameter>(BaseMat);
	PTopColor->ParameterName = TEXT("SkyTopColor");
	PTopColor->DefaultValue = FLinearColor(0.2f, 0.4f, 0.8f);
	BaseMat->GetExpressionCollection().AddExpression(PTopColor);

	auto* PMiddleColor = NewObject<UMaterialExpressionVectorParameter>(BaseMat);
	PMiddleColor->ParameterName = TEXT("SkyMiddleColor");
	PMiddleColor->DefaultValue = FLinearColor(0.4f, 0.6f, 1.0f);
	BaseMat->GetExpressionCollection().AddExpression(PMiddleColor);

	auto* PHorizonColor = NewObject<UMaterialExpressionVectorParameter>(BaseMat);
	PHorizonColor->ParameterName = TEXT("SkyHorizonColor");
	PHorizonColor->DefaultValue = FLinearColor(0.7f, 0.75f, 0.85f);
	BaseMat->GetExpressionCollection().AddExpression(PHorizonColor);

	auto* PBand1Color = NewObject<UMaterialExpressionVectorParameter>(BaseMat);
	PBand1Color->ParameterName = TEXT("SkyBand1Color");
	PBand1Color->DefaultValue = FLinearColor(0.5f, 0.6f, 0.9f);
	BaseMat->GetExpressionCollection().AddExpression(PBand1Color);

	auto* PSmogColor = NewObject<UMaterialExpressionVectorParameter>(BaseMat);
	PSmogColor->ParameterName = TEXT("SkySmogColor");
	PSmogColor->DefaultValue = FLinearColor(0.6f, 0.65f, 0.7f);
	BaseMat->GetExpressionCollection().AddExpression(PSmogColor);

	// Vertex color: R channel = height (0=horizon, 1=zenith)
	auto* VertexColor = NewObject<UMaterialExpressionVertexColor>(BaseMat);
	BaseMat->GetExpressionCollection().AddExpression(VertexColor);

	// Gradient: horizon → smog → band1 → middle → top
	// Using chained lerps based on vertex color R

	// Lower gradient: horizon → band1 using (height * 3) clamped
	auto* LerpLower = NewObject<UMaterialExpressionLinearInterpolate>(BaseMat);
	LerpLower->A.Connect(0, PHorizonColor);
	LerpLower->B.Connect(0, PBand1Color);
	LerpLower->Alpha.Connect(0, VertexColor); // R channel
	BaseMat->GetExpressionCollection().AddExpression(LerpLower);

	// Upper gradient: band1 → middle → top
	auto* LerpUpper = NewObject<UMaterialExpressionLinearInterpolate>(BaseMat);
	LerpUpper->A.Connect(0, PMiddleColor);
	LerpUpper->B.Connect(0, PTopColor);
	LerpUpper->Alpha.Connect(0, VertexColor);
	BaseMat->GetExpressionCollection().AddExpression(LerpUpper);

	// Final blend: lower half → upper half
	auto* LerpFinal = NewObject<UMaterialExpressionLinearInterpolate>(BaseMat);
	LerpFinal->A.Connect(0, LerpLower);
	LerpFinal->B.Connect(0, LerpUpper);
	LerpFinal->Alpha.Connect(0, VertexColor);
	BaseMat->GetExpressionCollection().AddExpression(LerpFinal);

	BaseMat->GetEditorOnlyData()->EmissiveColor.Connect(0, LerpFinal);
	BaseMat->PreEditChange(nullptr);
	BaseMat->PostEditChange();

	SkydomeMaterial = UMaterialInstanceDynamic::Create(BaseMat, this, TEXT("SkydomeMID"));

	SkydomeMesh = NewObject<UStaticMeshComponent>(this, TEXT("SkydomeComp"));
	SkydomeMesh->SetStaticMesh(Mesh);
	SkydomeMesh->SetMaterial(0, SkydomeMaterial);
	SkydomeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkydomeMesh->SetCastShadow(false);
	SkydomeMesh->bAffectDistanceFieldLighting = false;
	SkydomeMesh->RegisterComponent();
	SkydomeMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	UE_LOG(LogWowSky, Log, TEXT("Skydome mesh created: %d rings x %d segments"), NumRings, NumSegments);
}

void AWowSkyManager::CreateCloudMesh()
{
	const float CloudSize = 800000.0f;
	const float CloudHeight = 500000.0f;
	const int32 GridSize = 4;

	TArray<FVector> Verts;
	TArray<FVector4f> Colors;
	TArray<int32> Indices;

	for (int32 Y = 0; Y <= GridSize; ++Y)
	{
		for (int32 X = 0; X <= GridSize; ++X)
		{
			float FX = (static_cast<float>(X) / GridSize - 0.5f) * 2.0f * CloudSize;
			float FY = (static_cast<float>(Y) / GridSize - 0.5f) * 2.0f * CloudSize;
			Verts.Add(FVector(FX, FY, CloudHeight));

			float U = static_cast<float>(X) / GridSize;
			float V = static_cast<float>(Y) / GridSize;
			Colors.Add(FVector4f(U, V, 0.0f, 1.0f));
		}
	}

	for (int32 Y = 0; Y < GridSize; ++Y)
	{
		for (int32 X = 0; X < GridSize; ++X)
		{
			int32 I00 = Y * (GridSize + 1) + X;
			int32 I10 = I00 + 1;
			int32 I01 = I00 + (GridSize + 1);
			int32 I11 = I01 + 1;

			// Inverted winding (viewed from below)
			Indices.Add(I00);
			Indices.Add(I01);
			Indices.Add(I10);

			Indices.Add(I10);
			Indices.Add(I01);
			Indices.Add(I11);
		}
	}

	UStaticMesh* Mesh = BuildDomeMesh(this, Verts, Indices, Colors);

	// Cloud material: translucent, unlit, scrolling procedural noise
	UMaterial* BaseMat = NewObject<UMaterial>(this, TEXT("CloudMat_Base"));
	BaseMat->SetShadingModel(MSM_Unlit);
	BaseMat->TwoSided = true;
	BaseMat->BlendMode = BLEND_Translucent;

	auto* PCloudColor = NewObject<UMaterialExpressionVectorParameter>(BaseMat);
	PCloudColor->ParameterName = TEXT("CloudColor");
	PCloudColor->DefaultValue = FLinearColor(0.9f, 0.92f, 0.95f);
	BaseMat->GetExpressionCollection().AddExpression(PCloudColor);

	auto* PCloudOpacity = NewObject<UMaterialExpressionScalarParameter>(BaseMat);
	PCloudOpacity->ParameterName = TEXT("CloudOpacity");
	PCloudOpacity->DefaultValue = 0.3f;
	BaseMat->GetExpressionCollection().AddExpression(PCloudOpacity);

	// Procedural noise for cloud shapes
	auto* NoiseExpr = NewObject<UMaterialExpressionNoise>(BaseMat);
	NoiseExpr->Scale = 0.00002f;
	NoiseExpr->Levels = 4;
	NoiseExpr->OutputMin = 0.0f;
	NoiseExpr->OutputMax = 1.0f;
	BaseMat->GetExpressionCollection().AddExpression(NoiseExpr);

	// Panner for scrolling animation
	auto* Panner = NewObject<UMaterialExpressionPanner>(BaseMat);
	Panner->SpeedX = 0.005f;
	Panner->SpeedY = 0.003f;
	BaseMat->GetExpressionCollection().AddExpression(Panner);

	auto* TexCoord = NewObject<UMaterialExpressionTextureCoordinate>(BaseMat);
	BaseMat->GetExpressionCollection().AddExpression(TexCoord);
	Panner->Coordinate.Connect(0, TexCoord);

	NoiseExpr->Position.Connect(0, Panner);

	BaseMat->GetEditorOnlyData()->EmissiveColor.Connect(0, PCloudColor);
	BaseMat->GetEditorOnlyData()->Opacity.Connect(0, PCloudOpacity);
	BaseMat->PreEditChange(nullptr);
	BaseMat->PostEditChange();

	CloudMaterial = UMaterialInstanceDynamic::Create(BaseMat, this, TEXT("CloudMID"));

	CloudMesh = NewObject<UStaticMeshComponent>(this, TEXT("CloudComp"));
	CloudMesh->SetStaticMesh(Mesh);
	CloudMesh->SetMaterial(0, CloudMaterial);
	CloudMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CloudMesh->SetCastShadow(false);
	CloudMesh->bAffectDistanceFieldLighting = false;
	CloudMesh->RegisterComponent();
	CloudMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	UE_LOG(LogWowSky, Log, TEXT("Cloud mesh created"));
}

void AWowSkyManager::CreateCelestialDiscs()
{
	const float DiscDistance = 850000.0f; // Slightly inside skydome radius
	const float SunSize = 30000.0f;
	const float MoonSize = 20000.0f;

	// Helper: create a simple quad mesh for a celestial disc
	auto MakeDiscMesh = [this](float Size, const FName& Name) -> UStaticMesh*
	{
		TArray<FVector> Verts;
		TArray<FVector4f> Colors;
		TArray<int32> Indices;

		float H = Size * 0.5f;
		Verts.Add(FVector(0, -H, -H));
		Verts.Add(FVector(0,  H, -H));
		Verts.Add(FVector(0,  H,  H));
		Verts.Add(FVector(0, -H,  H));

		Colors.Add(FVector4f(0, 0, 1, 1));
		Colors.Add(FVector4f(1, 0, 1, 1));
		Colors.Add(FVector4f(1, 1, 1, 1));
		Colors.Add(FVector4f(0, 1, 1, 1));

		Indices.Add(0); Indices.Add(1); Indices.Add(2);
		Indices.Add(0); Indices.Add(2); Indices.Add(3);
		// Back face
		Indices.Add(0); Indices.Add(2); Indices.Add(1);
		Indices.Add(0); Indices.Add(3); Indices.Add(2);

		return BuildDomeMesh(this, Verts, Indices, Colors);
	};

	// Create emissive material for sun disc
	auto MakeDiscMaterial = [this](const FName& MatName, const FLinearColor& DefaultColor) -> UMaterialInstanceDynamic*
	{
		UMaterial* BaseMat = NewObject<UMaterial>(this, MatName);
		BaseMat->SetShadingModel(MSM_Unlit);
		BaseMat->TwoSided = true;
		BaseMat->BlendMode = BLEND_Additive;

		auto* PColor = NewObject<UMaterialExpressionVectorParameter>(BaseMat);
		PColor->ParameterName = TEXT("DiscColor");
		PColor->DefaultValue = DefaultColor;
		BaseMat->GetExpressionCollection().AddExpression(PColor);

		BaseMat->GetEditorOnlyData()->EmissiveColor.Connect(0, PColor);
		BaseMat->PreEditChange(nullptr);
		BaseMat->PostEditChange();

		return UMaterialInstanceDynamic::Create(BaseMat, this);
	};

	// Sun disc
	UStaticMesh* SunMesh = MakeDiscMesh(SunSize, TEXT("SunDiscQuad"));
	SunDiscMaterial = MakeDiscMaterial(TEXT("SunDiscMat"), FLinearColor(3.0f, 2.5f, 1.5f)); // Bright warm

	SunDiscMesh = NewObject<UStaticMeshComponent>(this, TEXT("SunDiscComp"));
	SunDiscMesh->SetStaticMesh(SunMesh);
	SunDiscMesh->SetMaterial(0, SunDiscMaterial);
	SunDiscMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SunDiscMesh->SetCastShadow(false);
	SunDiscMesh->bAffectDistanceFieldLighting = false;
	SunDiscMesh->RegisterComponent();
	SunDiscMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	// Moon disc
	UStaticMesh* MoonMeshAsset = MakeDiscMesh(MoonSize, TEXT("MoonDiscQuad"));
	MoonDiscMaterial = MakeDiscMaterial(TEXT("MoonDiscMat"), FLinearColor(0.6f, 0.65f, 0.9f)); // Cool blue-white

	MoonDiscMesh = NewObject<UStaticMeshComponent>(this, TEXT("MoonDiscComp"));
	MoonDiscMesh->SetStaticMesh(MoonMeshAsset);
	MoonDiscMesh->SetMaterial(0, MoonDiscMaterial);
	MoonDiscMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MoonDiscMesh->SetCastShadow(false);
	MoonDiscMesh->bAffectDistanceFieldLighting = false;
	MoonDiscMesh->RegisterComponent();
	MoonDiscMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	UE_LOG(LogWowSky, Log, TEXT("Celestial discs created (sun=%.0f, moon=%.0f)"), SunSize, MoonSize);
}

void AWowSkyManager::UpdateCelestialDiscs()
{
	const float DiscDistance = 850000.0f;

	FVector CameraPos = FVector::ZeroVector;
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		CameraPos = Pawn->GetActorLocation();
	}

	float NormalizedTime = TimeOfDay / 1440.0f;
	float SunPitch = -90.0f * FMath::Sin(NormalizedTime * PI);
	float SunYaw = (NormalizedTime * 360.0f) + 180.0f;
	bool bSunAboveHorizon = SunPitch < -5.0f;

	if (SunDiscMesh)
	{
		// Position sun disc along the sun's direction vector from camera
		FRotator SunRot(SunPitch, SunYaw, 0.0f);
		FVector SunDir = SunRot.Vector();
		FVector SunPos = CameraPos - SunDir * DiscDistance; // Light points toward origin

		SunDiscMesh->SetWorldLocation(SunPos);
		// Billboard: face the camera
		FRotator FaceCamera = (CameraPos - SunPos).Rotation();
		SunDiscMesh->SetWorldRotation(FaceCamera);
		SunDiscMesh->SetVisibility(bSunAboveHorizon);

		// Tint sun disc color based on time (warm at sunrise/sunset)
		if (SunDiscMaterial)
		{
			FLinearColor SunColor;
			if (bHasDbcLights)
			{
				SunColor = BlendZoneColor(LP_SunColor, CameraPos) * 3.0f;
			}
			else
			{
				SunColor = GetSunColorFallback() * 3.0f;
			}
			SunDiscMaterial->SetVectorParameterValue(TEXT("DiscColor"), SunColor);
		}
	}

	if (MoonDiscMesh)
	{
		// Moon is opposite the sun
		FRotator MoonRot(SunPitch + 180.0f, SunYaw + 180.0f, 0.0f);
		FVector MoonDir = MoonRot.Vector();
		FVector MoonPos = CameraPos - MoonDir * DiscDistance;

		MoonDiscMesh->SetWorldLocation(MoonPos);
		FRotator FaceCamera = (CameraPos - MoonPos).Rotation();
		MoonDiscMesh->SetWorldRotation(FaceCamera);
		MoonDiscMesh->SetVisibility(!bSunAboveHorizon);
	}
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

	if (TimeSpeed != 0.0f)
	{
		TimeOfDay += TimeSpeed * DeltaTime;
		while (TimeOfDay >= 1440.0f) TimeOfDay -= 1440.0f;
		while (TimeOfDay < 0.0f) TimeOfDay += 1440.0f;
	}

	// Keep skydome and clouds centered on camera
	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		FVector CamPos = Pawn->GetActorLocation();
		if (SkydomeMesh)
		{
			SkydomeMesh->SetWorldLocation(CamPos);
		}
		if (CloudMesh)
		{
			CloudMesh->SetWorldLocation(FVector(CamPos.X, CamPos.Y, 0.0f));
		}
	}

	UpdateSunPosition();
	UpdateLightColors();
	UpdateFog();
	UpdateSkydome();
	UpdateClouds();
	UpdateCelestialDiscs();
}

void AWowSkyManager::UpdateSunPosition()
{
	if (!SunLight) return;

	float NormalizedTime = TimeOfDay / 1440.0f;
	float SunPitch = -90.0f * FMath::Sin(NormalizedTime * PI);
	float SunYaw = (NormalizedTime * 360.0f) + 180.0f;

	SunLight->SetWorldRotation(FRotator(SunPitch, SunYaw, 0.0f));

	bool bSunAboveHorizon = SunPitch < -5.0f;
	SunLight->SetVisibility(bSunAboveHorizon);

	if (MoonLight)
	{
		MoonLight->SetWorldRotation(FRotator(SunPitch + 180.0f, SunYaw + 180.0f, 0.0f));
		MoonLight->SetVisibility(!bSunAboveHorizon);
	}
}

void AWowSkyManager::UpdateLightColors()
{
	if (!SunLight) return;

	float SunIntensity = GetSunIntensityForTime();

	if (bHasDbcLights)
	{
		FVector PlayerPos = FVector::ZeroVector;
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			PlayerPos = Pawn->GetActorLocation();
		}

		FLinearColor SunColor = BlendZoneColor(LP_SunColor, PlayerPos);
		FLinearColor AmbientColor = BlendZoneColor(LP_GlobalAmbient, PlayerPos);

		SunLight->SetLightColor(SunColor);
		SunLight->SetIntensity(SunIntensity);

		if (SkyLight)
		{
			SkyLight->SetLightColor(AmbientColor);
			SkyLight->SetIntensity(FMath::Lerp(0.15f, 1.0f, SunIntensity / 3.14f));
		}
	}
	else
	{
		SunLight->SetLightColor(GetSunColorFallback());
		SunLight->SetIntensity(SunIntensity);

		if (SkyLight)
		{
			SkyLight->SetIntensity(FMath::Lerp(0.15f, 1.0f, SunIntensity / 3.14f));
		}
	}
}

void AWowSkyManager::UpdateFog()
{
	if (!HeightFog) return;

	FLinearColor FogColor;
	float FogStartDist = 50000.0f;

	if (bHasDbcLights)
	{
		FVector PlayerPos = FVector::ZeroVector;
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			PlayerPos = Pawn->GetActorLocation();
		}
		FogColor = BlendZoneColor(LP_SkyFogColor, PlayerPos);

		if (bHasFloatParams)
		{
			float DbcFogDist = BlendZoneFloat(FP_FogDistance, PlayerPos);
			if (DbcFogDist > 0.0f)
			{
				FogStartDist = DbcFogDist * 100.0f; // yards → UE units
			}

			float FogMult = BlendZoneFloat(FP_FogMultiplier, PlayerPos);
			if (FogMult > 0.0f)
			{
				HeightFog->SetFogDensity(FogDensity * FogMult);
			}
		}
	}
	else
	{
		FogColor = GetFogColorFallback();
	}

	HeightFog->SetFogInscatteringColor(FogColor);
	HeightFog->SetStartDistance(FogStartDist);
}

void AWowSkyManager::UpdateSkydome()
{
	if (!SkydomeMaterial) return;

	FLinearColor TopColor, MiddleColor, HorizonColor, Band1Color, SmogColor;

	if (bHasDbcLights)
	{
		FVector PlayerPos = FVector::ZeroVector;
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			PlayerPos = Pawn->GetActorLocation();
		}

		TopColor = BlendZoneColor(LP_SkyTopColor, PlayerPos);
		MiddleColor = BlendZoneColor(LP_SkyMiddleColor, PlayerPos);
		Band1Color = BlendZoneColor(LP_SkyBand1, PlayerPos);
		SmogColor = BlendZoneColor(LP_SkySmogColor, PlayerPos);
		HorizonColor = BlendZoneColor(LP_SkyFogColor, PlayerPos);
	}
	else
	{
		FLinearColor Sky = GetSkyColorFallback();
		FLinearColor Fog = GetFogColorFallback();

		TopColor = Sky;
		MiddleColor = FMath::Lerp(Sky, Fog, 0.3f);
		Band1Color = FMath::Lerp(Sky, Fog, 0.5f);
		SmogColor = FMath::Lerp(Sky, Fog, 0.7f);
		HorizonColor = Fog;
	}

	SkydomeMaterial->SetVectorParameterValue(TEXT("SkyTopColor"), TopColor);
	SkydomeMaterial->SetVectorParameterValue(TEXT("SkyMiddleColor"), MiddleColor);
	SkydomeMaterial->SetVectorParameterValue(TEXT("SkyHorizonColor"), HorizonColor);
	SkydomeMaterial->SetVectorParameterValue(TEXT("SkyBand1Color"), Band1Color);
	SkydomeMaterial->SetVectorParameterValue(TEXT("SkySmogColor"), SmogColor);
}

void AWowSkyManager::UpdateClouds()
{
	if (!CloudMaterial) return;

	float CloudOpacity = 0.3f;

	if (bHasDbcLights && bHasFloatParams)
	{
		FVector PlayerPos = FVector::ZeroVector;
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			PlayerPos = Pawn->GetActorLocation();
		}

		float Density = BlendZoneFloat(FP_CloudDensity, PlayerPos);
		if (Density > 0.0f)
		{
			CloudOpacity = FMath::Clamp(Density, 0.0f, 1.0f);
		}
	}

	// Reduce cloud visibility at night
	float NightFactor = GetSunIntensityForTime() / 3.14f;
	CloudOpacity *= FMath::Lerp(0.15f, 1.0f, NightFactor);

	// Cloud color tinted by sky fog
	FLinearColor CloudColor;
	if (bHasDbcLights)
	{
		FVector PlayerPos = FVector::ZeroVector;
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			PlayerPos = Pawn->GetActorLocation();
		}
		CloudColor = FMath::Lerp(BlendZoneColor(LP_SkyFogColor, PlayerPos), FLinearColor::White, 0.5f);
	}
	else
	{
		CloudColor = FMath::Lerp(GetFogColorFallback(), FLinearColor::White, 0.5f);
	}

	CloudMaterial->SetVectorParameterValue(TEXT("CloudColor"), CloudColor);
	CloudMaterial->SetScalarParameterValue(TEXT("CloudOpacity"), CloudOpacity);
}

// ── DBC Color Interpolation ────────────────────────────────────────────────────

FLinearColor AWowSkyManager::InterpolateDbcColor(uint32 ParamID, ELightProperty Property) const
{
	if (ParamID == 0) return FLinearColor::White;

	const FDbcStore& Dbc = FDbcStore::Get();

	uint32 IntParamID = (ParamID - 1) * 18 + static_cast<uint32>(Property) + 1;
	const FLightIntParamsDbcEntry* Entry = Dbc.LightIntParams().GetById(IntParamID);
	if (!Entry || Entry->EntryCount == 0)
	{
		return FLinearColor::White;
	}

	uint32 DbcTime = static_cast<uint32>(TimeOfDay * 2.0f);
	uint32 Count = FMath::Min(Entry->EntryCount, static_cast<uint32>(FLightIntParamsDbcEntry::MaxEntries));

	// Find bracketing entries
	uint32 LowIdx = Count - 1;
	uint32 HighIdx = 0;
	bool bFoundHigh = false;

	for (uint32 i = 0; i < Count; i++)
	{
		if (Entry->Times[i] <= DbcTime)
		{
			LowIdx = i;
		}
		if (!bFoundHigh && Entry->Times[i] > DbcTime)
		{
			HighIdx = i;
			bFoundHigh = true;
		}
	}

	// If no entry found above current time, wrap to first entry (next day)
	if (!bFoundHigh)
	{
		HighIdx = 0;
	}

	auto DecodeColor = [](uint32 Packed) -> FLinearColor
	{
		float B = ((Packed >> 0) & 0xFF) / 255.0f;
		float G = ((Packed >> 8) & 0xFF) / 255.0f;
		float R = ((Packed >> 16) & 0xFF) / 255.0f;
		return FLinearColor(R, G, B);
	};

	FLinearColor LowColor = DecodeColor(Entry->Values[LowIdx]);
	FLinearColor HighColor = DecodeColor(Entry->Values[HighIdx]);

	uint32 LowTime = Entry->Times[LowIdx];
	uint32 HighTime = Entry->Times[HighIdx];

	if (HighTime <= LowTime)
	{
		HighTime += 2880;
		uint32 AdjTime = (DbcTime < LowTime) ? DbcTime + 2880 : DbcTime;
		float Alpha = (HighTime > LowTime) ? static_cast<float>(AdjTime - LowTime) / (HighTime - LowTime) : 0.0f;
		return FMath::Lerp(LowColor, HighColor, FMath::Clamp(Alpha, 0.0f, 1.0f));
	}

	float Alpha = static_cast<float>(DbcTime - LowTime) / FMath::Max(1u, HighTime - LowTime);
	return FMath::Lerp(LowColor, HighColor, FMath::Clamp(Alpha, 0.0f, 1.0f));
}

float AWowSkyManager::InterpolateDbcFloat(uint32 ParamID, ELightFloatProperty Property) const
{
	if (ParamID == 0) return 0.0f;

	const FDbcStore& Dbc = FDbcStore::Get();

	uint32 FloatParamID = (ParamID - 1) * 6 + static_cast<uint32>(Property) + 1;
	const FLightFloatParamsDbcEntry* Entry = Dbc.LightFloatParams().GetById(FloatParamID);
	if (!Entry || Entry->EntryCount == 0)
	{
		return 0.0f;
	}

	uint32 DbcTime = static_cast<uint32>(TimeOfDay * 2.0f);
	uint32 Count = FMath::Min(Entry->EntryCount, static_cast<uint32>(FLightFloatParamsDbcEntry::MaxEntries));

	uint32 LowIdx = Count - 1;
	uint32 HighIdx = 0;
	bool bFoundHigh = false;

	for (uint32 i = 0; i < Count; i++)
	{
		if (Entry->Times[i] <= DbcTime)
		{
			LowIdx = i;
		}
		if (!bFoundHigh && Entry->Times[i] > DbcTime)
		{
			HighIdx = i;
			bFoundHigh = true;
		}
	}

	if (!bFoundHigh)
	{
		HighIdx = 0;
	}

	float LowVal = Entry->Values[LowIdx];
	float HighVal = Entry->Values[HighIdx];
	uint32 LowTime = Entry->Times[LowIdx];
	uint32 HighTime = Entry->Times[HighIdx];

	if (HighTime <= LowTime)
	{
		HighTime += 2880;
		uint32 AdjTime = (DbcTime < LowTime) ? DbcTime + 2880 : DbcTime;
		float Alpha = (HighTime > LowTime) ? static_cast<float>(AdjTime - LowTime) / (HighTime - LowTime) : 0.0f;
		return FMath::Lerp(LowVal, HighVal, FMath::Clamp(Alpha, 0.0f, 1.0f));
	}

	float Alpha = static_cast<float>(DbcTime - LowTime) / FMath::Max(1u, HighTime - LowTime);
	return FMath::Lerp(LowVal, HighVal, FMath::Clamp(Alpha, 0.0f, 1.0f));
}

FLinearColor AWowSkyManager::BlendZoneColor(ELightProperty Property, const FVector& PlayerPos) const
{
	if (MapLights.Num() == 0)
	{
		switch (Property)
		{
		case LP_SunColor: return GetSunColorFallback();
		case LP_SkyFogColor: return GetFogColorFallback();
		default: return GetSkyColorFallback();
		}
	}

	FVector WowPos = FWowCoordinate::UEToWow(PlayerPos);
	FLinearColor Result = FLinearColor::Black;
	float TotalWeight = 0.0f;
	const FLightDbcEntry* DefaultLight = nullptr;

	for (const FLightDbcEntry* Light : MapLights)
	{
		if (Light->FalloffEnd == 0.0f)
		{
			DefaultLight = Light;
			continue;
		}

		FVector LightPos(Light->X, Light->Y, Light->Z);
		float Dist = FVector::Dist(WowPos, LightPos);
		if (Dist > Light->FalloffEnd) continue;

		float Weight = 1.0f;
		if (Dist > Light->FalloffStart && Light->FalloffEnd > Light->FalloffStart)
		{
			Weight = 1.0f - (Dist - Light->FalloffStart) / (Light->FalloffEnd - Light->FalloffStart);
		}

		uint32 ParamID = Light->ParamIDs[0];
		if (ParamID == 0) continue;

		Result += InterpolateDbcColor(ParamID, Property) * Weight;
		TotalWeight += Weight;
	}

	if (TotalWeight < 1.0f && DefaultLight)
	{
		uint32 DefaultParamID = DefaultLight->ParamIDs[0];
		if (DefaultParamID != 0)
		{
			float DefaultWeight = 1.0f - TotalWeight;
			Result += InterpolateDbcColor(DefaultParamID, Property) * DefaultWeight;
			TotalWeight += DefaultWeight;
		}
	}

	if (TotalWeight > 0.0f)
	{
		Result /= TotalWeight;
	}
	else
	{
		switch (Property)
		{
		case LP_SunColor: return GetSunColorFallback();
		case LP_SkyFogColor: return GetFogColorFallback();
		default: return GetSkyColorFallback();
		}
	}

	return Result;
}

float AWowSkyManager::BlendZoneFloat(ELightFloatProperty Property, const FVector& PlayerPos) const
{
	if (MapLights.Num() == 0 || !bHasFloatParams) return 0.0f;

	FVector WowPos = FWowCoordinate::UEToWow(PlayerPos);
	float Result = 0.0f;
	float TotalWeight = 0.0f;
	const FLightDbcEntry* DefaultLight = nullptr;

	for (const FLightDbcEntry* Light : MapLights)
	{
		if (Light->FalloffEnd == 0.0f)
		{
			DefaultLight = Light;
			continue;
		}

		FVector LightPos(Light->X, Light->Y, Light->Z);
		float Dist = FVector::Dist(WowPos, LightPos);
		if (Dist > Light->FalloffEnd) continue;

		float Weight = 1.0f;
		if (Dist > Light->FalloffStart && Light->FalloffEnd > Light->FalloffStart)
		{
			Weight = 1.0f - (Dist - Light->FalloffStart) / (Light->FalloffEnd - Light->FalloffStart);
		}

		uint32 ParamID = Light->ParamIDs[0];
		if (ParamID == 0) continue;

		Result += InterpolateDbcFloat(ParamID, Property) * Weight;
		TotalWeight += Weight;
	}

	if (TotalWeight < 1.0f && DefaultLight)
	{
		uint32 DefaultParamID = DefaultLight->ParamIDs[0];
		if (DefaultParamID != 0)
		{
			float DefaultWeight = 1.0f - TotalWeight;
			Result += InterpolateDbcFloat(DefaultParamID, Property) * DefaultWeight;
			TotalWeight += DefaultWeight;
		}
	}

	return (TotalWeight > 0.0f) ? Result / TotalWeight : 0.0f;
}

// ── Fallback (hardcoded) color functions ────────────────────────────────────────

FLinearColor AWowSkyManager::GetSunColorFallback() const
{
	float T = TimeOfDay;
	if (T >= 300.0f && T < 420.0f)
	{
		float A = (T - 300.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(0.9f, 0.4f, 0.2f), FLinearColor(1.0f, 0.95f, 0.85f), A);
	}
	else if (T >= 420.0f && T < 1020.0f)
	{
		return FLinearColor(1.0f, 0.95f, 0.85f);
	}
	else if (T >= 1020.0f && T < 1140.0f)
	{
		float A = (T - 1020.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(1.0f, 0.95f, 0.85f), FLinearColor(0.9f, 0.4f, 0.2f), A);
	}
	return FLinearColor(0.3f, 0.35f, 0.5f);
}

float AWowSkyManager::GetSunIntensityForTime() const
{
	float T = TimeOfDay;
	if (T >= 300.0f && T < 420.0f)
	{
		return FMath::Lerp(0.1f, 3.14f, (T - 300.0f) / 120.0f);
	}
	else if (T >= 420.0f && T < 1020.0f)
	{
		return 3.14f;
	}
	else if (T >= 1020.0f && T < 1140.0f)
	{
		return FMath::Lerp(3.14f, 0.1f, (T - 1020.0f) / 120.0f);
	}
	return 0.1f;
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
	return FLinearColor(0.05f, 0.05f, 0.1f);
}

FLinearColor AWowSkyManager::GetSkyColorFallback() const
{
	float T = TimeOfDay;
	if (T >= 420.0f && T < 1020.0f)
	{
		return FLinearColor(0.4f, 0.6f, 1.0f);
	}
	else if (T >= 300.0f && T < 420.0f)
	{
		float A = (T - 300.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(0.1f, 0.1f, 0.2f), FLinearColor(0.4f, 0.6f, 1.0f), A);
	}
	else if (T >= 1020.0f && T < 1140.0f)
	{
		float A = (T - 1020.0f) / 120.0f;
		return FMath::Lerp(FLinearColor(0.4f, 0.6f, 1.0f), FLinearColor(0.1f, 0.1f, 0.2f), A);
	}
	return FLinearColor(0.1f, 0.1f, 0.2f);
}
