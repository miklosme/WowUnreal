#include "WowDoodadManager.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "Formats/M2Parser.h"
#include "Formats/M2Types.h"
#include "Formats/BlpParser.h"
#include "Formats/BlpTypes.h"
#include "WowTextureFactory.h"
#include "WowTerrainMaterial.h"
#include "Coord/WowCoordinate.h"
#include "Formats/AdtTypes.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowDoodad, Log, All);

TMap<FString, TSharedPtr<FM2Data>> FWowDoodadManager::ParsedM2Cache;
FCriticalSection FWowDoodadManager::CacheLock;

FString FWowDoodadManager::GetSkinPath(const FString& M2Path)
{
    // WoW 3.3.5 skin file: replace .m2 or .M2 with 00.skin
    FString SkinPath = M2Path;
    if (SkinPath.EndsWith(TEXT(".m2"), ESearchCase::IgnoreCase))
    {
        SkinPath = SkinPath.Left(SkinPath.Len() - 3);
    }
    SkinPath += TEXT("00.skin");
    return SkinPath;
}

TSharedPtr<FM2Data> FWowDoodadManager::GetOrParseM2(const FString& M2Path, FMpqManager* Mpq)
{
    if (!Mpq) return nullptr;

    FString NormalizedPath = M2Path.ToLower();
    NormalizedPath.ReplaceInline(TEXT("/"), TEXT("\\"));

    {
        FScopeLock Lock(&CacheLock);
        TSharedPtr<FM2Data>* Found = ParsedM2Cache.Find(NormalizedPath);
        if (Found)
        {
            return *Found;
        }
    }

    // Read M2 file
    TArray<uint8> M2Raw;
    if (!Mpq->ReadFile(M2Path, M2Raw))
    {
        UE_LOG(LogWowDoodad, Verbose, TEXT("Failed to read M2: %s"), *M2Path);
        return nullptr;
    }

    // Read skin file
    FString SkinPath = GetSkinPath(M2Path);
    TArray<uint8> SkinRaw;
    if (!Mpq->ReadFile(SkinPath, SkinRaw))
    {
        UE_LOG(LogWowDoodad, Verbose, TEXT("Failed to read skin: %s"), *SkinPath);
        return nullptr;
    }

    // Parse
    TSharedPtr<FM2Data> Data = MakeShared<FM2Data>(FM2Parser::Parse(M2Raw, SkinRaw));
    if (!Data->bIsValid)
    {
        UE_LOG(LogWowDoodad, Verbose, TEXT("Failed to parse M2: %s"), *M2Path);
        return nullptr;
    }

    {
        FScopeLock Lock(&CacheLock);
        ParsedM2Cache.Add(NormalizedPath, Data);
    }

    UE_LOG(LogWowDoodad, Verbose, TEXT("Parsed M2: %s (%d verts, %d indices, %d textures)"),
        *M2Path, Data->Vertices.Num(), Data->Indices.Num(), Data->TexturePaths.Num());

    return Data;
}

UProceduralMeshComponent* FWowDoodadManager::CreateM2MeshComponent(
    AActor* Owner, const FM2Data& Data, const FString& M2Path,
    FMpqManager* Mpq, FWowAssetCache* Cache, FName CompName)
{
    if (Data.Vertices.Num() == 0 || Data.Indices.Num() == 0)
    {
        return nullptr;
    }

    UProceduralMeshComponent* MeshComp = NewObject<UProceduralMeshComponent>(Owner, CompName);
    MeshComp->SetupAttachment(Owner->GetRootComponent());

    // Convert M2 vertices to ProceduralMesh format
    const int32 NumVerts = Data.Vertices.Num();
    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FProcMeshTangent> Tangents;

    Vertices.SetNum(NumVerts);
    Normals.SetNum(NumVerts);
    UVs.SetNum(NumVerts);
    Tangents.SetNum(NumVerts);

    for (int32 i = 0; i < NumVerts; ++i)
    {
        const FM2Vertex& V = Data.Vertices[i];
        // file→UE: (fileY, -fileX, fileZ) matching WowGodot's approach
        Vertices[i] = FVector(V.Position.Y, -V.Position.X, V.Position.Z) * FWowCoordinate::SCALE;
        Normals[i] = FVector(V.Normal.Y, -V.Normal.X, V.Normal.Z);
        Normals[i].Normalize();
        UVs[i] = V.TexCoord;

        // Approximate tangent
        FVector N = Normals[i];
        FVector T = FVector::CrossProduct(N, FVector(0, 1, 0));
        if (T.SizeSquared() < 0.001f)
        {
            T = FVector::CrossProduct(N, FVector(1, 0, 0));
        }
        T.Normalize();
        Tangents[i] = FProcMeshTangent(T, false);
    }

    // Reverse winding order: WoW files use CW front-faces (DirectX/LH convention)
    // but UE uses CCW front-faces. Swap i1 and i2 in each triangle.
    TArray<int32> Indices;
    Indices.SetNum(Data.Indices.Num());
    for (int32 i = 0; i + 2 < Data.Indices.Num(); i += 3)
    {
        Indices[i]     = static_cast<int32>(Data.Indices[i]);
        Indices[i + 1] = static_cast<int32>(Data.Indices[i + 2]);
        Indices[i + 2] = static_cast<int32>(Data.Indices[i + 1]);
    }

    TArray<FLinearColor> EmptyColors;
    MeshComp->CreateMeshSection_LinearColor(0, Vertices, Indices, Normals, UVs, EmptyColors, Tangents, false);

    // Load and apply the first texture as material
    if (Data.TexturePaths.Num() > 0 && Mpq && Cache)
    {
        const FString& TexPath = Data.TexturePaths[0];
        if (!TexPath.IsEmpty())
        {
            UTexture2D* Tex = Cache->FindTexture(TexPath);
            if (!Tex)
            {
                TArray<uint8> BlpRaw;
                if (Mpq->ReadFile(TexPath, BlpRaw))
                {
                    FBlpTexture BlpData = FBlpParser::Parse(BlpRaw);
                    if (BlpData.bIsValid)
                    {
                        Tex = FWowTextureFactory::CreateTexture(BlpData, TexPath);
                        if (Tex) Cache->CacheTexture(TexPath, Tex);
                    }
                }
            }

            if (Tex)
            {
                // Use the terrain material which has a BaseTexture parameter
                UMaterial* BaseMat = FWowTerrainMaterial::GetBaseMaterial();
                if (BaseMat)
                {
                    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, Owner);
                    MID->SetTextureParameterValue(FName(TEXT("BaseTexture")), Tex);
                    MeshComp->SetMaterial(0, MID);
                }
            }
        }
    }

    MeshComp->SetCastShadow(true);
    MeshComp->RegisterComponent();

    return MeshComp;
}

void FWowDoodadManager::SpawnDoodads(AActor* ParentActor, const TArray<FAdtDoodadPlacement>& Placements,
                                      const TArray<FString>& DoodadPaths, FMpqManager* Mpq, FWowAssetCache* Cache)
{
    if (!ParentActor || !Mpq || Placements.Num() == 0)
    {
        return;
    }

    UE_LOG(LogWowDoodad, Log, TEXT("Spawning %d doodads"), Placements.Num());

    // Track unique IDs to avoid spawning duplicates (shared across tiles)
    TSet<uint32> SpawnedIds;
    int32 Spawned = 0;

    for (int32 i = 0; i < Placements.Num(); ++i)
    {
        const FAdtDoodadPlacement& Placement = Placements[i];

        // Skip duplicates
        if (SpawnedIds.Contains(Placement.UniqueId))
        {
            continue;
        }
        SpawnedIds.Add(Placement.UniqueId);

        // Resolve path
        if (Placement.NameIndex < 0 || Placement.NameIndex >= DoodadPaths.Num())
        {
            continue;
        }

        const FString& M2Path = DoodadPaths[Placement.NameIndex];
        if (M2Path.IsEmpty())
        {
            continue;
        }

        // Parse M2
        TSharedPtr<FM2Data> M2Data = GetOrParseM2(M2Path, Mpq);
        if (!M2Data || !M2Data->bIsValid)
        {
            continue;
        }

        // Create mesh component
        FName CompName = *FString::Printf(TEXT("Doodad_%d_%d"), Placement.UniqueId, i);
        UProceduralMeshComponent* MeshComp = CreateM2MeshComponent(
            ParentActor, *M2Data, M2Path, Mpq, Cache, CompName);

        if (!MeshComp)
        {
            continue;
        }

        // MDDF positions: convert to ADT space, then AdtToUE (matches terrain)
        float AdtX = Placement.Position.X;
        float AdtY = Placement.Position.Y;
        float AdtZ = Placement.Position.Z;
        FVector UEPos = FWowCoordinate::AdtToUE(AdtX, AdtY, AdtZ);

        // Rotation: WowGodot uses Godot YXZ euler with angles (rotX, rotY-90, -rotZ).
        // Map Godot axes to UE axes and build quaternion in correct order.
        const float Deg2Rad = PI / 180.0f;
        float GodotRotX = Placement.Rotation.X * Deg2Rad;
        float GodotRotY = (Placement.Rotation.Y - 90.0f) * Deg2Rad;
        float GodotRotZ = -Placement.Rotation.Z * Deg2Rad;

        FQuat QYaw   = FQuat(FVector(0, 0, 1), GodotRotY);
        FQuat QRoll  = FQuat(FVector(1, 0, 0), GodotRotX);
        FQuat QPitch = FQuat(FVector(0, 1, 0), -GodotRotZ);
        FQuat FinalRot = QPitch * QRoll * QYaw;

        // Scale
        float ScaleVal = Placement.GetScaleFloat();

        MeshComp->SetWorldLocation(UEPos);
        MeshComp->SetWorldRotation(FinalRot);
        MeshComp->SetWorldScale3D(FVector(ScaleVal));

        ++Spawned;
    }

    UE_LOG(LogWowDoodad, Log, TEXT("Spawned %d doodad meshes"), Spawned);
}

UProceduralMeshComponent* FWowDoodadManager::SpawnSingleDoodad(
    AActor* ParentActor, const FAdtDoodadPlacement& Placement,
    const FString& M2Path, FMpqManager* Mpq, FWowAssetCache* Cache)
{
    if (!ParentActor || !Mpq || M2Path.IsEmpty())
    {
        return nullptr;
    }

    TSharedPtr<FM2Data> M2Data = GetOrParseM2(M2Path, Mpq);
    if (!M2Data || !M2Data->bIsValid)
    {
        return nullptr;
    }

    FName CompName = *FString::Printf(TEXT("Doodad_%u"), Placement.UniqueId);
    UProceduralMeshComponent* MeshComp = CreateM2MeshComponent(
        ParentActor, *M2Data, M2Path, Mpq, Cache, CompName);

    if (!MeshComp)
    {
        return nullptr;
    }

    // MDDF positions: convert to ADT space, then AdtToUE (matches terrain)
    float AdtX = Placement.Position.X;
    float AdtY = Placement.Position.Y;
    float AdtZ = Placement.Position.Z;
    FVector UEPos = FWowCoordinate::AdtToUE(AdtX, AdtY, AdtZ);

    // Rotation: WowGodot uses Godot YXZ euler with angles (rotX, rotY-90, -rotZ).
    const float Deg2Rad = PI / 180.0f;
    float GodotRotX = Placement.Rotation.X * Deg2Rad;
    float GodotRotY = (Placement.Rotation.Y - 90.0f) * Deg2Rad;
    float GodotRotZ = -Placement.Rotation.Z * Deg2Rad;

    FQuat QYaw   = FQuat(FVector(0, 0, 1), GodotRotY);
    FQuat QRoll  = FQuat(FVector(1, 0, 0), GodotRotX);
    FQuat QPitch = FQuat(FVector(0, 1, 0), -GodotRotZ);
    FQuat FinalRot = QPitch * QRoll * QYaw;

    float ScaleVal = Placement.GetScaleFloat();

    MeshComp->SetWorldLocation(UEPos);
    MeshComp->SetWorldRotation(FinalRot);
    MeshComp->SetWorldScale3D(FVector(ScaleVal));

    return MeshComp;
}
