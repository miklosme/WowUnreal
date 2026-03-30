#include "WowDoodadManager.h"
#include "Mpq/MpqManager.h"
#include "WowAssetCache.h"
#include "WowSkeletalMeshBuilder.h"
#include "Formats/M2Parser.h"
#include "Formats/M2Types.h"
#include "Formats/BlpParser.h"
#include "Formats/BlpTypes.h"
#include "WowTextureFactory.h"
#include "WowTerrainMaterial.h"
#include "Coord/WowCoordinate.h"
#include "Formats/AdtTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WowM2ParticleComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowDoodad, Log, All);

TMap<FString, TSharedPtr<FM2Data>> FWowDoodadManager::ParsedM2Cache;
FCriticalSection FWowDoodadManager::CacheLock;
TMap<FString, TWeakObjectPtr<USkeleton>> FWowDoodadManager::SkeletonCache;
TMap<FString, TArray<TWeakObjectPtr<UAnimSequence>>> FWowDoodadManager::AnimCache;

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

        // Check if this model should use skeletal mesh (animated)
        TSharedPtr<FM2Data> M2Parsed = GetOrParseM2(M2Path, Mpq);
        if (!M2Parsed || !M2Parsed->bIsValid)
        {
            continue;
        }

        // MDDF positions: convert to ADT space, then AdtToUE (matches terrain)
        float AdtX = Placement.Position.X;
        float AdtY = Placement.Position.Y;
        float AdtZ = Placement.Position.Z;
        FVector UEPos = FWowCoordinate::AdtToUE(AdtX, AdtY, AdtZ);
        float ScaleVal = Placement.GetScaleFloat();
        FRotator Rot = FWowCoordinate::WowRotationToUE(
            Placement.Rotation.X, Placement.Rotation.Y, Placement.Rotation.Z);

        if (ShouldUseSkeletalMesh(*M2Parsed))
        {
            // Animated M2 — use skeletal mesh with animations
            USkeletalMesh* SkelMesh = GetOrCreateSkeletalMesh(M2Path, Mpq, Cache);
            if (!SkelMesh) continue;

            FName CompName = *FString::Printf(TEXT("SkelDoodad_%d_%d"), Placement.UniqueId, i);
            USkeletalMeshComponent* SkelComp = NewObject<USkeletalMeshComponent>(ParentActor, CompName);
            SkelComp->SetSkeletalMesh(SkelMesh);
            SkelComp->SetupAttachment(ParentActor->GetRootComponent());
            SkelComp->SetCastShadow(true);
            SkelComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            SkelComp->SetWorldLocation(UEPos);
            SkelComp->SetWorldRotation(Rot);
            SkelComp->SetWorldScale3D(FVector(ScaleVal));
            SkelComp->RegisterComponent();

            // Play first animation if available
            FString NormPath = M2Path.ToLower();
            NormPath.ReplaceInline(TEXT("/"), TEXT("\\"));
            auto* Anims = AnimCache.Find(NormPath);
            if (Anims && Anims->Num() > 0 && (*Anims)[0].IsValid())
            {
                SkelComp->PlayAnimation((*Anims)[0].Get(), true);
            }
        }
        else
        {
            // Static M2 — use static mesh as before
            UStaticMesh* SM = GetOrCreateStaticMesh(M2Path, Mpq, Cache);
            if (!SM) continue;

            FName CompName = *FString::Printf(TEXT("Doodad_%d_%d"), Placement.UniqueId, i);
            UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(ParentActor, CompName);
            MeshComp->SetStaticMesh(SM);
            MeshComp->SetupAttachment(ParentActor->GetRootComponent());
            MeshComp->SetCastShadow(true);
            MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            MeshComp->SetWorldLocation(UEPos);
            MeshComp->SetWorldRotation(Rot);
            MeshComp->SetWorldScale3D(FVector(ScaleVal));
            MeshComp->RegisterComponent();
        }

        // Add particle effects if the M2 has particle emitters
        if (M2Parsed->ParticleEmitters.Num() > 0)
        {
            FName ParticleCompName = *FString::Printf(TEXT("Particles_%d_%d"), Placement.UniqueId, i);
            UWowM2ParticleComponent* ParticleComp = NewObject<UWowM2ParticleComponent>(ParentActor, ParticleCompName);
            ParticleComp->SetupAttachment(ParentActor->GetRootComponent());
            ParticleComp->SetWorldLocation(UEPos);
            ParticleComp->SetWorldRotation(Rot);
            ParticleComp->SetWorldScale3D(FVector(ScaleVal));
            ParticleComp->RegisterComponent();

            // Initialize particles from M2 data
            ParticleComp->InitializeFromParsedM2Data(M2Parsed.Get());

            UE_LOG(LogWowDoodad, Log, TEXT("Added %d particle emitters to doodad %s"),
                M2Parsed->ParticleEmitters.Num(), *M2Path);
        }

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
    MeshComp->SetWorldRotation(FWowCoordinate::WowRotationToUE(
        Placement.Rotation.X, Placement.Rotation.Y, Placement.Rotation.Z));
    MeshComp->SetWorldScale3D(FVector(ScaleVal));
    MeshComp->RegisterComponent();

    return MeshComp;
}

UTexture2D* FWowDoodadManager::LoadBlpTexture(const FString& Path, FMpqManager* Mpq, FWowAssetCache* Cache)
{
    if (Path.IsEmpty() || !Mpq || !Cache) return nullptr;

    UTexture2D* Tex = Cache->FindTexture(Path);
    if (Tex) return Tex;

    TArray<uint8> BlpRaw;
    if (!Mpq->ReadFile(Path, BlpRaw))
    {
        // Log failed texture reads for debugging
        static int32 sFailLog = 0;
        if (sFailLog < 10)
        {
            sFailLog++;
            UE_LOG(LogWowDoodad, Warning, TEXT("Failed to read BLP from MPQ: '%s' (exists=%d)"),
                *Path, Mpq->FileExists(Path) ? 1 : 0);
        }
        return nullptr;
    }

    FBlpTexture BlpData = FBlpParser::Parse(BlpRaw);
    if (!BlpData.bIsValid) return nullptr;

    Tex = FWowTextureFactory::CreateTexture(BlpData, Path);
    if (Tex) Cache->CacheTexture(Path, Tex);
    return Tex;
}

UStaticMesh* FWowDoodadManager::CreateStaticMeshFromM2(const FM2Data& Data, const FString& M2Path,
                                                         FMpqManager* Mpq, FWowAssetCache* Cache)
{
    if (Data.Vertices.Num() == 0 || Data.Indices.Num() == 0)
    {
        return nullptr;
    }

    UStaticMesh* StaticMesh = NewObject<UStaticMesh>();
    StaticMesh->bAllowCPUAccess = true; // Required for HISMC instancing

    FMeshDescription MeshDesc;
    FStaticMeshAttributes Attributes(MeshDesc);
    Attributes.Register();

    const int32 NumVerts = Data.Vertices.Num();

    // Determine polygon groups from render passes + submeshes
    // Each render pass maps to a submesh (triangle range) and a texture
    // Group by unique (submesh, texture) pairs to create material slots
    struct FMaterialBatch
    {
        int32 StartIndex;   // Index into Data.Indices
        int32 NumIndices;   // Number of indices (NumTriangles * 3... well, nTriangles from skin is actually index count)
        int32 TextureIndex; // Index into Data.TexturePaths
    };

    TArray<FMaterialBatch> Batches;
    bool bHasValidBatches = Data.RenderPasses.Num() > 0 && Data.Submeshes.Num() > 0;

    if (bHasValidBatches)
    {
        for (const FM2RenderPass& Pass : Data.RenderPasses)
        {
            if (Pass.SubmeshIndex >= Data.Submeshes.Num()) continue;

            const FM2Submesh& Sub = Data.Submeshes[Pass.SubmeshIndex];
            // StartTriangle and NumTriangles in M2 skin format are index counts, not triangle counts
            int32 Start = static_cast<int32>(Sub.StartTriangle);
            int32 Count = static_cast<int32>(Sub.NumTriangles);

            if (Start + Count > Data.Indices.Num()) continue;
            if (Count < 3) continue;

            FMaterialBatch Batch;
            Batch.StartIndex = Start;
            Batch.NumIndices = Count;
            Batch.TextureIndex = Pass.TextureIndex;
            Batches.Add(Batch);
        }
    }

    // Fallback: single batch with all triangles if no valid render passes
    if (Batches.Num() == 0)
    {
        FMaterialBatch Batch;
        Batch.StartIndex = 0;
        Batch.NumIndices = Data.Indices.Num();
        Batch.TextureIndex = 0;
        Batches.Add(Batch);
    }

    // Create polygon groups (one per batch)
    TArray<FPolygonGroupID> PolyGroups;
    for (int32 i = 0; i < Batches.Num(); ++i)
    {
        PolyGroups.Add(MeshDesc.CreatePolygonGroup());
    }

    MeshDesc.ReserveNewVertices(NumVerts);
    MeshDesc.ReserveNewVertexInstances(NumVerts);

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

        // WoW model space → UE local: (Y, X, Z) * SCALE
        // Same derivation as WMO: model→ADT is (X,Z,-Y), ADT→UE is (-Z,X,Y)
        VertexPositions[VertID] = FVector3f(V.Position.Y * FWowCoordinate::SCALE,
                                             V.Position.X * FWowCoordinate::SCALE,
                                             V.Position.Z * FWowCoordinate::SCALE);

        FVertexInstanceID InstID = MeshDesc.CreateVertexInstance(VertID);
        VertexInstanceIDs[i] = InstID;

        FVector3f Normal(V.Normal.Y, V.Normal.X, V.Normal.Z);
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

    // Create triangles per batch (polygon group)
    for (int32 BatchIdx = 0; BatchIdx < Batches.Num(); ++BatchIdx)
    {
        const FMaterialBatch& Batch = Batches[BatchIdx];
        int32 NumBatchTris = Batch.NumIndices / 3;

        for (int32 t = 0; t < NumBatchTris; ++t)
        {
            int32 BaseIdx = Batch.StartIndex + t * 3;
            if (BaseIdx + 2 >= Data.Indices.Num()) break;

            uint16 I0 = Data.Indices[BaseIdx];
            uint16 I1 = Data.Indices[BaseIdx + 1];
            uint16 I2 = Data.Indices[BaseIdx + 2];

            if (I0 >= NumVerts || I1 >= NumVerts || I2 >= NumVerts) continue;

            TArray<FVertexInstanceID> TriVerts;
            TriVerts.Add(VertexInstanceIDs[I0]);
            TriVerts.Add(VertexInstanceIDs[I1]);
            TriVerts.Add(VertexInstanceIDs[I2]);
            MeshDesc.CreatePolygon(PolyGroups[BatchIdx], TriVerts);
        }
    }

    // Build materials and collect them for post-build assignment
    UMaterial* BaseMat = FWowTerrainMaterial::GetSimpleObjectMaterial();
    UMaterial* FallbackMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));

    TArray<UMaterialInterface*> BatchMaterials;
    if (Mpq && Cache && BaseMat)
    {
        for (int32 BatchIdx = 0; BatchIdx < Batches.Num(); ++BatchIdx)
        {
            const FMaterialBatch& Batch = Batches[BatchIdx];
            UTexture2D* Tex = nullptr;

            if (Batch.TextureIndex >= 0 && Batch.TextureIndex < Data.TexturePaths.Num())
            {
                Tex = LoadBlpTexture(Data.TexturePaths[Batch.TextureIndex], Mpq, Cache);
            }

            // If texture failed to load, try alternate paths
            if (!Tex && Batch.TextureIndex >= 0 && Batch.TextureIndex < Data.TexturePaths.Num())
            {
                const FString& FailedPath = Data.TexturePaths[Batch.TextureIndex];
                FString TexFilename = FPaths::GetCleanFilename(FailedPath);

                // Try 1: look in the M2 model's own directory
                FString M2Dir = FPaths::GetPath(M2Path);
                Tex = LoadBlpTexture(M2Dir / TexFilename, Mpq, Cache);

                // Try 2: Interface/ textures often have duplicates in World/ paths
                if (!Tex && FailedPath.Contains(TEXT("Interface"), ESearchCase::IgnoreCase))
                {
                    // Try the StormWind passive doodads directory (common for Elwynn trees)
                    Tex = LoadBlpTexture(FString(TEXT("World/Generic/Human/Passive Doodads/StormWind/")) + TexFilename, Mpq, Cache);
                }

                // Try 3: look in parent directories of the M2
                if (!Tex)
                {
                    FString ParentDir = FPaths::GetPath(M2Dir);
                    Tex = LoadBlpTexture(ParentDir / TexFilename, Mpq, Cache);
                }
            }

            // Always create a proper MID — use texture if available, otherwise
            // create a solid-color material (brown for trunks, grey for generic)
            UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, StaticMesh);
            if (Tex)
            {
                MID->SetTextureParameterValue(FName(TEXT("Layer0Texture")), Tex);
            }
            BatchMaterials.Add(MID);
        }
    }
    else
    {
        for (int32 i = 0; i < Batches.Num(); ++i)
        {
            BatchMaterials.Add(FallbackMat);
        }
    }

    // Add materials BEFORE build
    for (UMaterialInterface* Mat : BatchMaterials)
    {
        StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Mat));
    }

    // Build static mesh from description
    TArray<const FMeshDescription*> MeshDescs;
    MeshDescs.Add(&MeshDesc);

    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = true;
    Params.bCommitMeshDescription = true;
    StaticMesh->BuildFromMeshDescriptions(MeshDescs, Params);
    StaticMesh->NeverStream = true;
    StaticMesh->InitResources();
    for (FStaticMaterial& Mat : StaticMesh->GetStaticMaterials()) { Mat.UVChannelData.bInitialized = true; }

    // Log material state after build to diagnose
    static bool bLoggedMatState = false;
    if (!bLoggedMatState)
    {
        bLoggedMatState = true;
        UE_LOG(LogWowDoodad, Warning, TEXT("After build: %d static materials, %d batch materials"),
            StaticMesh->GetStaticMaterials().Num(), BatchMaterials.Num());
        for (int32 i = 0; i < StaticMesh->GetStaticMaterials().Num(); ++i)
        {
            UMaterialInterface* M = StaticMesh->GetStaticMaterials()[i].MaterialInterface;
            UE_LOG(LogWowDoodad, Warning, TEXT("  mat[%d] = %s"), i, M ? *M->GetName() : TEXT("null"));
        }
    }

    // Re-set materials after build in case BuildFromMeshDescriptions resets them
    for (int32 i = 0; i < BatchMaterials.Num(); ++i)
    {
        if (StaticMesh->GetStaticMaterials().Num() <= i)
        {
            StaticMesh->GetStaticMaterials().SetNum(i + 1);
        }
        StaticMesh->GetStaticMaterials()[i].MaterialInterface = BatchMaterials[i];
    }

    // Ensure bounds are computed — bFastBuild may skip this
    StaticMesh->CalculateExtendedBounds();

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

TArray<UInstancedStaticMeshComponent*> FWowDoodadManager::SpawnDoodadsInstanced(
    AActor* ParentActor, const TArray<FAdtDoodadPlacement>& Placements,
    const TArray<FString>& DoodadPaths, FMpqManager* Mpq, FWowAssetCache* Cache)
{
    TArray<UInstancedStaticMeshComponent*> Result;

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

        // Use plain ISM instead of HISMC — more reliable bounds for runtime meshes
        FName CompName = *FString::Printf(TEXT("ISM_%s"), *FPaths::GetBaseFilename(M2Path));
        UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(ParentActor, CompName);
        ISM->SetupAttachment(ParentActor->GetRootComponent());
        ISM->SetStaticMesh(SM);
        ISM->SetCastShadow(false);
        ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        ISM->SetMobility(EComponentMobility::Static);
        ISM->bNeverDistanceCull = true;

        // Override materials on the ISM component
        for (int32 MatIdx = 0; MatIdx < SM->GetStaticMaterials().Num(); ++MatIdx)
        {
            if (SM->GetStaticMaterials()[MatIdx].MaterialInterface)
            {
                ISM->SetMaterial(MatIdx, SM->GetStaticMaterials()[MatIdx].MaterialInterface);
            }
        }

        // Must register BEFORE adding instances so the component transform is set
        ISM->RegisterComponent();

        // Add all instances for this model
        for (const FAdtDoodadPlacement* Placement : GroupPlacements)
        {
            FVector UEPos = FWowCoordinate::AdtToUE(
                Placement->Position.X, Placement->Position.Y, Placement->Position.Z);
            float ScaleVal = Placement->GetScaleFloat();
            FRotator Rot = FWowCoordinate::WowRotationToUE(
                Placement->Rotation.X, Placement->Rotation.Y, Placement->Rotation.Z);

            FTransform InstanceTransform(Rot, UEPos, FVector(ScaleVal));
            ISM->AddInstance(InstanceTransform, /*bWorldSpace=*/true);
        }

        ISM->MarkRenderStateDirty();

        Result.Add(ISM);
        TotalInstances += GroupPlacements.Num();
    }

    UE_LOG(LogWowDoodad, Log, TEXT("Instanced %d doodads across %d HISMCs (%d unique models)"),
        TotalInstances, Result.Num(), GroupedPlacements.Num());

    return Result;
}

bool FWowDoodadManager::ShouldUseSkeletalMesh(const FM2Data& Data)
{
    // Temporarily disabled: skeletal mesh GPU skin crashes on some M2 models
    return false;
    // return Data.HasBones() && Data.HasAnimationData() && Data.Bones.Num() > 1;
}

USkeletalMesh* FWowDoodadManager::GetOrCreateSkeletalMesh(const FString& M2Path, FMpqManager* Mpq, FWowAssetCache* Cache)
{
    if (!Mpq || !Cache) return nullptr;

    FString NormalizedPath = M2Path.ToLower();
    NormalizedPath.ReplaceInline(TEXT("/"), TEXT("\\"));

    // Check cache first
    USkeletalMesh* Cached = Cache->FindSkelMesh(NormalizedPath);
    if (Cached) return Cached;

    // Parse M2
    TSharedPtr<FM2Data> M2Data = GetOrParseM2(M2Path, Mpq);
    if (!M2Data || !M2Data->bIsValid || !ShouldUseSkeletalMesh(*M2Data))
    {
        return nullptr;
    }

    FString ModelName = FPaths::GetBaseFilename(M2Path);

    // Create or find skeleton
    USkeleton* Skeleton = nullptr;
    {
        auto* CachedSkel = SkeletonCache.Find(NormalizedPath);
        if (CachedSkel && CachedSkel->IsValid())
        {
            Skeleton = CachedSkel->Get();
        }
        else
        {
            Skeleton = FWowSkeletalMeshBuilder::CreateSkeleton(*M2Data, ModelName);
            if (Skeleton)
            {
                SkeletonCache.Add(NormalizedPath, Skeleton);
            }
        }
    }

    if (!Skeleton) return nullptr;

    // Create skeletal mesh
    USkeletalMesh* SkelMesh = FWowSkeletalMeshBuilder::CreateSkeletalMesh(*M2Data, Skeleton, ModelName, Mpq, Cache);
    if (SkelMesh)
    {
        Cache->CacheSkelMesh(NormalizedPath, SkelMesh);

        // Create and cache animations
        auto* CachedAnims = AnimCache.Find(NormalizedPath);
        if (!CachedAnims)
        {
            TArray<UAnimSequence*> Anims = FWowSkeletalMeshBuilder::CreateAnimations(*M2Data, Skeleton, ModelName);
            TArray<TWeakObjectPtr<UAnimSequence>> WeakAnims;
            for (UAnimSequence* Anim : Anims)
            {
                WeakAnims.Add(Anim);
            }
            AnimCache.Add(NormalizedPath, MoveTemp(WeakAnims));
        }

        UE_LOG(LogWowDoodad, Log, TEXT("Created skeletal mesh for %s with %d bones, %d animations"),
            *M2Path, M2Data->Bones.Num(), M2Data->AnimationTracks.Num());
    }

    return SkelMesh;
}
