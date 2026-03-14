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
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
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

        // Get or create static mesh (cached)
        UStaticMesh* SM = GetOrCreateStaticMesh(M2Path, Mpq, Cache);
        if (!SM)
        {
            continue;
        }

        // Create mesh component
        FName CompName = *FString::Printf(TEXT("Doodad_%d_%d"), Placement.UniqueId, i);
        UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(ParentActor, CompName);
        MeshComp->SetStaticMesh(SM);
        MeshComp->SetupAttachment(ParentActor->GetRootComponent());
        MeshComp->SetCastShadow(true);
        MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        // MDDF positions: convert to ADT space, then AdtToUE (matches terrain)
        float AdtX = Placement.Position.X;
        float AdtY = Placement.Position.Y;
        float AdtZ = Placement.Position.Z;
        FVector UEPos = FWowCoordinate::AdtToUE(AdtX, AdtY, AdtZ);

        // Simple rotation: just yaw
        float ScaleVal = Placement.GetScaleFloat();

        MeshComp->SetWorldLocation(UEPos);
        MeshComp->SetWorldRotation(FRotator(0.0f, -Placement.Rotation.Y, 0.0f));
        MeshComp->SetWorldScale3D(FVector(ScaleVal));
        MeshComp->RegisterComponent();

        ++Spawned;
    }

    UE_LOG(LogWowDoodad, Log, TEXT("Spawned %d doodad meshes"), Spawned);
}

UStaticMeshComponent* FWowDoodadManager::SpawnSingleDoodad(
    AActor* ParentActor, const FAdtDoodadPlacement& Placement,
    const FString& M2Path, FMpqManager* Mpq, FWowAssetCache* Cache)
{
    if (!ParentActor || !Mpq || M2Path.IsEmpty())
    {
        return nullptr;
    }

    // Get or create static mesh (cached)
    UStaticMesh* SM = GetOrCreateStaticMesh(M2Path, Mpq, Cache);
    if (!SM)
    {
        return nullptr;
    }

    FName CompName = *FString::Printf(TEXT("Doodad_%u"), Placement.UniqueId);
    UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(ParentActor, CompName);
    MeshComp->SetStaticMesh(SM);
    MeshComp->SetupAttachment(ParentActor->GetRootComponent());
    MeshComp->SetCastShadow(true);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // MDDF positions: convert to ADT space, then AdtToUE (matches terrain)
    float AdtX = Placement.Position.X;
    float AdtY = Placement.Position.Y;
    float AdtZ = Placement.Position.Z;
    FVector UEPos = FWowCoordinate::AdtToUE(AdtX, AdtY, AdtZ);

    float ScaleVal = Placement.GetScaleFloat();

    MeshComp->SetWorldLocation(UEPos);
    MeshComp->SetWorldRotation(FRotator(0.0f, -Placement.Rotation.Y, 0.0f));
    MeshComp->SetWorldScale3D(FVector(ScaleVal));
    MeshComp->RegisterComponent();

    return MeshComp;
}

UStaticMesh* FWowDoodadManager::CreateStaticMeshFromM2(const FM2Data& Data, const FString& M2Path,
                                                         FMpqManager* Mpq, FWowAssetCache* Cache)
{
    if (Data.Vertices.Num() == 0 || Data.Indices.Num() == 0)
    {
        return nullptr;
    }

    UStaticMesh* StaticMesh = NewObject<UStaticMesh>();
    StaticMesh->AddToRoot(); // Prevent GC

    FMeshDescription MeshDesc;
    FStaticMeshAttributes Attributes(MeshDesc);
    Attributes.Register();

    const int32 NumVerts = Data.Vertices.Num();
    const int32 NumTris = Data.Indices.Num() / 3;

    // Reserve geometry
    FPolygonGroupID PolyGroup = MeshDesc.CreatePolygonGroup();
    MeshDesc.ReserveNewVertices(NumVerts);
    MeshDesc.ReserveNewVertexInstances(NumVerts);
    MeshDesc.ReserveNewPolygons(NumTris);
    MeshDesc.ReserveNewEdges(Data.Indices.Num());

    TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
    TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
    TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
    TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();

    // Create vertices and vertex instances
    TArray<FVertexInstanceID> VertexInstanceIDs;
    VertexInstanceIDs.SetNum(NumVerts);

    for (int32 i = 0; i < NumVerts; ++i)
    {
        const FM2Vertex& V = Data.Vertices[i];
        FVertexID VertID = MeshDesc.CreateVertex();

        // WoW Z-up RH → UE Z-up LH: negate X
        VertexPositions[VertID] = FVector3f(-V.Position.X * FWowCoordinate::SCALE,
                                             V.Position.Y * FWowCoordinate::SCALE,
                                             V.Position.Z * FWowCoordinate::SCALE);

        FVertexInstanceID InstID = MeshDesc.CreateVertexInstance(VertID);
        VertexInstanceIDs[i] = InstID;

        FVector3f Normal(-V.Normal.X, V.Normal.Y, V.Normal.Z);
        Normal.Normalize();
        VertexInstanceNormals[InstID] = Normal;

        // Approximate tangent
        FVector3f T = FVector3f::CrossProduct(Normal, FVector3f(0, 1, 0));
        if (T.SizeSquared() < 0.001f)
        {
            T = FVector3f::CrossProduct(Normal, FVector3f(1, 0, 0));
        }
        T.Normalize();
        VertexInstanceTangents[InstID] = T;
        VertexInstanceBinormalSigns[InstID] = 1.0f;

        VertexInstanceUVs.Set(InstID, 0, FVector2f(V.TexCoord.X, V.TexCoord.Y));
    }

    // Create triangles
    for (int32 i = 0; i < NumTris; ++i)
    {
        TArray<FVertexInstanceID> TriVerts;
        TriVerts.Add(VertexInstanceIDs[Data.Indices[i * 3]]);
        TriVerts.Add(VertexInstanceIDs[Data.Indices[i * 3 + 1]]);
        TriVerts.Add(VertexInstanceIDs[Data.Indices[i * 3 + 2]]);
        MeshDesc.CreatePolygon(PolyGroup, TriVerts);
    }

    // Build static mesh from description
    TArray<const FMeshDescription*> MeshDescs;
    MeshDescs.Add(&MeshDesc);

    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = true;
    StaticMesh->BuildFromMeshDescriptions(MeshDescs, Params);

    // Apply material with first texture
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
                UMaterial* BaseMat = FWowTerrainMaterial::GetBaseMaterial();
                if (BaseMat)
                {
                    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, StaticMesh);
                    MID->SetTextureParameterValue(FName(TEXT("BaseTexture")), Tex);
                    StaticMesh->SetMaterial(0, MID);
                }
            }
        }
    }

    return StaticMesh;
}

UStaticMesh* FWowDoodadManager::GetOrCreateStaticMesh(const FString& M2Path, FMpqManager* Mpq, FWowAssetCache* Cache)
{
    if (!Mpq || !Cache) return nullptr;

    FString NormalizedPath = M2Path.ToLower();
    NormalizedPath.ReplaceInline(TEXT("/"), TEXT("\\"));

    // Check cache first
    UStaticMesh* Cached = Cache->FindMesh(NormalizedPath);
    if (Cached) return Cached;

    // Parse M2
    TSharedPtr<FM2Data> M2Data = GetOrParseM2(M2Path, Mpq);
    if (!M2Data || !M2Data->bIsValid) return nullptr;

    // Create static mesh
    UStaticMesh* SM = CreateStaticMeshFromM2(*M2Data, M2Path, Mpq, Cache);
    if (SM)
    {
        Cache->CacheMesh(NormalizedPath, SM);
        UE_LOG(LogWowDoodad, Log, TEXT("Created StaticMesh for %s (%d verts)"), *M2Path, M2Data->Vertices.Num());
    }

    return SM;
}

TArray<UHierarchicalInstancedStaticMeshComponent*> FWowDoodadManager::SpawnDoodadsInstanced(
    AActor* ParentActor, const TArray<FAdtDoodadPlacement>& Placements,
    const TArray<FString>& DoodadPaths, FMpqManager* Mpq, FWowAssetCache* Cache)
{
    TArray<UHierarchicalInstancedStaticMeshComponent*> Result;

    if (!ParentActor || !Mpq || !Cache || Placements.Num() == 0)
    {
        return Result;
    }

    // Group placements by M2 path
    TMap<FString, TArray<const FAdtDoodadPlacement*>> GroupedPlacements;
    for (const FAdtDoodadPlacement& Placement : Placements)
    {
        if (Placement.NameIndex < 0 || Placement.NameIndex >= DoodadPaths.Num()) continue;
        const FString& Path = DoodadPaths[Placement.NameIndex];
        if (Path.IsEmpty()) continue;

        GroupedPlacements.FindOrAdd(Path).Add(&Placement);
    }

    int32 TotalInstances = 0;

    for (auto& Pair : GroupedPlacements)
    {
        const FString& M2Path = Pair.Key;
        const TArray<const FAdtDoodadPlacement*>& GroupPlacements = Pair.Value;

        // Get or create static mesh for this model
        UStaticMesh* SM = GetOrCreateStaticMesh(M2Path, Mpq, Cache);
        if (!SM) continue;

        // Create HISMC
        FName CompName = *FString::Printf(TEXT("HISMC_%s"), *FPaths::GetBaseFilename(M2Path));
        UHierarchicalInstancedStaticMeshComponent* HISMC = NewObject<UHierarchicalInstancedStaticMeshComponent>(ParentActor, CompName);
        HISMC->SetupAttachment(ParentActor->GetRootComponent());
        HISMC->SetStaticMesh(SM);
        HISMC->SetCastShadow(true);
        HISMC->SetCullDistances(0.0f, 200000.0f); // 2km cull distance
        HISMC->bDisableCollision = true;

        // Add all instances for this model
        for (const FAdtDoodadPlacement* Placement : GroupPlacements)
        {
            FVector UEPos = FWowCoordinate::AdtToUE(
                Placement->Position.X, Placement->Position.Y, Placement->Position.Z);
            float ScaleVal = Placement->GetScaleFloat();
            FRotator Rot(0.0f, -Placement->Rotation.Y, 0.0f);

            FTransform InstanceTransform(Rot, UEPos, FVector(ScaleVal));
            HISMC->AddInstance(InstanceTransform, /*bWorldSpace=*/true);
        }

        HISMC->RegisterComponent();
        Result.Add(HISMC);
        TotalInstances += GroupPlacements.Num();
    }

    UE_LOG(LogWowDoodad, Log, TEXT("Instanced %d doodads across %d HISMCs (%d unique models)"),
        TotalInstances, Result.Num(), GroupedPlacements.Num());

    return Result;
}
