#include "WowDoodadManager.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "Formats/M2Parser.h"
#include "Formats/M2Types.h"
#include "Formats/BlpParser.h"
#include "Formats/BlpTypes.h"
#include "WowTextureFactory.h"
#include "Coord/WowCoordinate.h"
#include "Formats/AdtTypes.h"
#include "ProceduralMeshComponent.h"

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
        // M2 model vertices are in model-local noggit3-like space
        // NoggitToUE: UE.X = -Ng.Z*SCALE, UE.Y = Ng.X*SCALE, UE.Z = Ng.Y*SCALE
        Vertices[i] = FVector(-V.Position.Z, V.Position.X, V.Position.Y) * FWowCoordinate::SCALE;
        Normals[i] = FVector(-V.Normal.Z, V.Normal.X, V.Normal.Y);
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

    // Convert indices from uint16 to int32
    TArray<int32> Indices;
    Indices.SetNum(Data.Indices.Num());
    for (int32 i = 0; i < Data.Indices.Num(); ++i)
    {
        Indices[i] = static_cast<int32>(Data.Indices[i]);
    }

    TArray<FLinearColor> EmptyColors;
    MeshComp->CreateMeshSection_LinearColor(0, Vertices, Indices, Normals, UVs, EmptyColors, Tangents, false);

    // Try to load and apply the first texture as material
    if (Data.TexturePaths.Num() > 0 && Mpq && Cache)
    {
        const FString& TexPath = Data.TexturePaths[0];
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
                    if (Tex)
                    {
                        Cache->CacheTexture(TexPath, Tex);
                    }
                }
            }
        }

        // Use a simple material with the texture
        if (Tex)
        {
            UMaterial* BaseMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
            if (BaseMat)
            {
                MeshComp->SetMaterial(0, BaseMat);
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

        // MDDF positions are in noggit3 space (X=east, Y=up, Z=south)
        FVector UEPos = FWowCoordinate::NoggitToUE(
            Placement.Position.X, Placement.Position.Y, Placement.Position.Z);

        // Rotation: MDDF stores rotation in degrees
        // For now use a simple mapping
        FRotator UERot = FRotator(0.0f, -Placement.Rotation.Y, 0.0f);

        // Scale
        float ScaleVal = Placement.GetScaleFloat();

        MeshComp->SetWorldLocation(UEPos);
        MeshComp->SetWorldRotation(UERot);
        MeshComp->SetWorldScale3D(FVector(ScaleVal));

        ++Spawned;
    }

    UE_LOG(LogWowDoodad, Log, TEXT("Spawned %d doodad meshes"), Spawned);
}
