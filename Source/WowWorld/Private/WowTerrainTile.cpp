#include "WowTerrainTile.h"
#include "WowTerrainMeshBuilder.h"
#include "WowTerrainMaterial.h"
#include "Formats/AdtTypes.h"
#include "Coord/WowCoordinate.h"
#include "WowDoodadManager.h"
#include "WowWmoRenderer.h"
#include "WowWaterRenderer.h"
#include "VT/RuntimeVirtualTexture.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

DEFINE_LOG_CATEGORY_STATIC(LogTerrainTile, Log, All);

AWowTerrainTile::AWowTerrainTile()
{
    PrimaryActorTick.bCanEverTick = false;
    RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(RootScene);
}

UMaterialInterface* AWowTerrainTile::GetDefaultTerrainMaterial() const
{
    static UMaterial* DefaultMat = nullptr;
    if (!DefaultMat)
    {
        DefaultMat = LoadObject<UMaterial>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
    }
    return DefaultMat;
}

void AWowTerrainTile::BuildFromAdtData(const FAdtData& Data, int32 TX, int32 TY, FMpqManager* Mpq, FWowAssetCache* Cache, TSet<uint32>* SpawnedWmoIds)
{
    TileCoord = FIntPoint(TX, TY);
    if (!Data.bIsValid) return;

    FVector TileCenter = FWowCoordinate::TileToWorld(TX, TY);
    SetActorLocation(TileCenter);

    UE_LOG(LogTerrainTile, Log, TEXT("Tile %d,%d at %s: %d textures, %d doodads, %d WMOs"),
        TX, TY, *TileCenter.ToString(), Data.TexturePaths.Num(), Data.DoodadPlacements.Num(), Data.WmoPlacements.Num());

    UMaterialInterface* DefaultMat = GetDefaultTerrainMaterial();

    // Build a single UStaticMesh with one polygon group per chunk (up to 256 material slots)
    FMeshDescription MeshDesc;
    // Must set UV channels before Register() so attributes are allocated correctly
    MeshDesc.SetNumUVChannels(2); // UV0 = alpha/splatmap [0,1], UV1 = world-space tiled
    FStaticMeshAttributes Attributes(MeshDesc);
    Attributes.Register();

    TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
    TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
    TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
    TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
    TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();

    // Collect materials per section
    TArray<UMaterialInterface*> SectionMaterials;
    int32 SectionIndex = 0;
    int32 ChunksWithHeights = 0;
    int32 ChunksSkipped = 0;
    int32 ChunksTextured = 0;

    for (int32 i = 0; i < 256; ++i)
    {
        const FAdtChunkData& Chunk = Data.Chunks[i];
        FTerrainChunkMeshData MeshData = FTerrainMeshBuilder::BuildChunkMesh(Chunk, TX, TY);

        if (MeshData.Vertices.Num() == 0 || MeshData.Indices.Num() == 0)
        {
            ChunksSkipped++;
            continue;
        }

        // Check if this chunk has valid height variation
        bool bHasHeights = false;
        for (int32 h = 0; h < 145; h++)
        {
            if (Chunk.Heights[h] != 0.0f) { bHasHeights = true; break; }
        }
        if (bHasHeights) ChunksWithHeights++;

        // Create a polygon group for this chunk (maps to a material slot)
        FPolygonGroupID PolyGroup = MeshDesc.CreatePolygonGroup();

        // Add vertices and vertex instances for this chunk
        int32 BaseVertIndex = MeshDesc.Vertices().Num();
        TArray<FVertexInstanceID> ChunkVertInstIDs;
        ChunkVertInstIDs.SetNum(MeshData.Vertices.Num());

        for (int32 v = 0; v < MeshData.Vertices.Num(); ++v)
        {
            const FVector& P = MeshData.Vertices[v];
            FVertexID VertID = MeshDesc.CreateVertex();
            VertexPositions[VertID] = FVector3f(P.X, P.Y, P.Z);

            FVertexInstanceID InstID = MeshDesc.CreateVertexInstance(VertID);
            ChunkVertInstIDs[v] = InstID;

            FVector3f N = (v < MeshData.Normals.Num())
                ? FVector3f(MeshData.Normals[v]) : FVector3f(0, 0, 1);
            N.Normalize();
            VertexInstanceNormals[InstID] = N;

            FVector3f T = FVector3f::CrossProduct(N, FVector3f(0, 1, 0));
            if (T.SizeSquared() < 0.001f)
                T = FVector3f::CrossProduct(N, FVector3f(1, 0, 0));
            T.Normalize();
            VertexInstanceTangents[InstID] = T;
            VertexInstanceBinormalSigns[InstID] = 1.0f;

            FVector2f UV0 = (v < MeshData.UVs.Num())
                ? FVector2f(MeshData.UVs[v]) : FVector2f(0, 0);
            VertexInstanceUVs.Set(InstID, 0, UV0);

            FVector2f UV1 = (v < MeshData.TiledUVs.Num())
                ? FVector2f(MeshData.TiledUVs[v]) : FVector2f(0, 0);
            VertexInstanceUVs.Set(InstID, 1, UV1);
        }

        // Create triangles for this chunk
        for (int32 t = 0; t < MeshData.Indices.Num(); t += 3)
        {
            TArray<FVertexInstanceID> TriVerts;
            TriVerts.Add(ChunkVertInstIDs[MeshData.Indices[t]]);
            TriVerts.Add(ChunkVertInstIDs[MeshData.Indices[t + 1]]);
            TriVerts.Add(ChunkVertInstIDs[MeshData.Indices[t + 2]]);
            MeshDesc.CreatePolygon(PolyGroup, TriVerts);
        }

        // Determine material for this chunk
        UMaterialInstanceDynamic* ChunkMat = FWowTerrainMaterial::CreateChunkMaterial(
            Chunk, Data, Mpq, Cache, this);

        if (ChunkMat)
        {
            SectionMaterials.Add(ChunkMat);
            ChunksTextured++;
        }
        else
        {
            SectionMaterials.Add(DefaultMat);
        }

        SectionIndex++;
    }

    if (SectionIndex > 0)
    {
        // Build UStaticMesh from the mesh description
        UStaticMesh* SM = NewObject<UStaticMesh>();

        // Add materials BEFORE building so sections are initialized with correct materials
        for (UMaterialInterface* Mat : SectionMaterials)
        {
            SM->GetStaticMaterials().Add(FStaticMaterial(Mat));
        }

        SM->bAllowCPUAccess = true;

        TArray<const FMeshDescription*> MeshDescs;
        MeshDescs.Add(&MeshDesc);
        UStaticMesh::FBuildMeshDescriptionsParams Params;
        Params.bBuildSimpleCollision = false;
        Params.bFastBuild = true;
        Params.bCommitMeshDescription = true;
        SM->BuildFromMeshDescriptions(MeshDescs, Params);

        // Enable complex collision (use render mesh as collision)
        if (SM->GetBodySetup() == nullptr)
        {
            SM->CreateBodySetup();
        }
        if (UBodySetup* BodySetup = SM->GetBodySetup())
        {
            BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
            // Skip CreatePhysicsMeshes — line traces work with complex-as-simple
            // without cooked physics data, and cooking 6400 meshes freezes the game
        }

        // Create UStaticMeshComponent and set materials on the component
        UStaticMeshComponent* MeshComp = NewObject<UStaticMeshComponent>(this, TEXT("TerrainMesh"));
        MeshComp->SetStaticMesh(SM);
        MeshComp->SetupAttachment(RootScene);
        MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        MeshComp->SetCollisionResponseToAllChannels(ECR_Block);
        for (int32 s = 0; s < SectionMaterials.Num(); ++s)
        {
            MeshComp->SetMaterial(s, SectionMaterials[s]);
        }
        MeshComp->RegisterComponent();
        MeshComp->SetCastShadow(false);
        MeshComp->SetCollisionObjectType(ECC_WorldStatic);

        ChunkMeshes.Add(MeshComp);
        UE_LOG(LogTerrainTile, Log, TEXT("Tile %d,%d: %d sections, %d textured, %d/%d have heights, %d skipped"),
            TX, TY, SectionIndex, ChunksTextured, ChunksWithHeights, 256 - ChunksSkipped, ChunksSkipped);
    }

    // Spawn water meshes from MH2O data
    {
        TArray<UStaticMeshComponent*> WaterComps = FWowWaterRenderer::CreateWaterMeshes(this, Data, TX, TY);
        for (UStaticMeshComponent* WC : WaterComps)
        {
            WaterMeshes.Add(WC);
        }

        // Check for ocean liquid type and spawn ocean plane if found
        float OceanHeight = 0.0f;
        bool bHasOcean = false;
        for (int32 i = 0; i < 256; i++)
        {
            for (const FMH2OInstance& Layer : Data.WaterChunks[i].Layers)
            {
                if (Layer.GetLiquidCategory() == 1) // ocean
                {
                    OceanHeight = Layer.MinHeight;
                    bHasOcean = true;
                    break;
                }
            }
            if (bHasOcean) break;
        }
        if (bHasOcean)
        {
            UStaticMeshComponent* OceanComp = FWowWaterRenderer::CreateOceanPlane(this, OceanHeight, TX, TY);
            if (OceanComp)
            {
                WaterMeshes.Add(OceanComp);
            }
        }
    }

    // Spawn doodads using HISMC instancing (batched per unique M2 model)
    if (Data.DoodadPlacements.Num() > 0)
    {
        InstancedDoodads = FWowDoodadManager::SpawnDoodadsInstanced(
            this, Data.DoodadPlacements, Data.DoodadPaths, Mpq, Cache);
        bUsesInstancedDoodads = (InstancedDoodads.Num() > 0);
    }

    // Store WMO placement data for distance-based streaming (doodads are now instanced)
    WmoPlacements = Data.WmoPlacements;
    WmoPaths = Data.WmoPaths;
    CachedMpq = Mpq;
    CachedCache = Cache;

    // Store area IDs per chunk for zone detection
    ChunkAreaIds.SetNum(256);
    for (int32 i = 0; i < 256; ++i)
    {
        ChunkAreaIds[i] = Data.Chunks[i].AreaId;
    }

    UE_LOG(LogTerrainTile, Log, TEXT("Tile %d,%d: %d instanced doodad HISMCs, %d WMO placements for streaming"),
        TX, TY, InstancedDoodads.Num(), WmoPlacements.Num());
}

void AWowTerrainTile::ApplyRuntimeVirtualTexture(URuntimeVirtualTexture* RVT)
{
    if (!RVT) return;

    // Configure all terrain chunk meshes to render into the RVT
    for (UStaticMeshComponent* Mesh : ChunkMeshes)
    {
        if (Mesh)
        {
            Mesh->RuntimeVirtualTextures.AddUnique(RVT);
            Mesh->VirtualTextureLodBias = 0;
            Mesh->VirtualTextureCullMips = 0;
            Mesh->VirtualTextureMinCoverage = 1;
            Mesh->VirtualTextureRenderPassType = ERuntimeVirtualTextureMainPassType::Always;
            Mesh->MarkRenderStateDirty();
        }
    }

    UE_LOG(LogTerrainTile, Log, TEXT("Tile %d,%d: Applied RVT to %d chunk meshes"),
        TileCoord.X, TileCoord.Y, ChunkMeshes.Num());
}
