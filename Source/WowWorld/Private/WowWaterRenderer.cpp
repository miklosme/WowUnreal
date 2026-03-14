#include "WowWaterRenderer.h"
#include "Formats/AdtTypes.h"
#include "Coord/WowCoordinate.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Materials/MaterialInstanceDynamic.h"

DEFINE_LOG_CATEGORY_STATIC(LogWowWater, Log, All);

TArray<UStaticMeshComponent*> FWowWaterRenderer::CreateWaterMeshes(
	AActor* Owner, const FAdtData& AdtData, int32 TileX, int32 TileY)
{
	TArray<UStaticMeshComponent*> Result;
	if (!Owner) return Result;

	int32 WaterSectionIndex = 0;

	for (int32 ChunkIdx = 0; ChunkIdx < 256; ChunkIdx++)
	{
		const FMH2OChunkData& WaterChunk = AdtData.WaterChunks[ChunkIdx];
		if (!WaterChunk.HasWater()) continue;

		for (const FMH2OInstance& Layer : WaterChunk.Layers)
		{
			if (Layer.Width == 0 || Layer.Height == 0) continue;

			// Figure out chunk position: chunk (ChunkX, ChunkY) within tile
			int32 ChunkX = ChunkIdx % 16;
			int32 ChunkY = ChunkIdx / 16;

			// ADT-space origin for this chunk
			float ChunkNgX = TileX * FWowCoordinate::TILE_SIZE + ChunkX * FWowCoordinate::CHUNK_SIZE;
			float ChunkNgZ = TileY * FWowCoordinate::TILE_SIZE + ChunkY * FWowCoordinate::CHUNK_SIZE;

			// Sub-tile size within a chunk (chunk is 8x8 sub-tiles)
			float SubTileSize = FWowCoordinate::CHUNK_SIZE / 8.0f;

			// Build vertices for the liquid grid
			int32 VW = Layer.Width + 1;
			int32 VH = Layer.Height + 1;

			TArray<FVector> Vertices;
			TArray<FVector2D> UVs;
			Vertices.Reserve(VW * VH);
			UVs.Reserve(VW * VH);

			for (int32 Row = 0; Row <= Layer.Height; Row++)
			{
				for (int32 Col = 0; Col <= Layer.Width; Col++)
				{
					float NgX = ChunkNgX + (Layer.XOffset + Col) * SubTileSize;
					float NgZ = ChunkNgZ + (Layer.YOffset + Row) * SubTileSize;

					int32 HeightIdx = Row * VW + Col;
					float NgY = (HeightIdx < Layer.Heights.Num()) ? Layer.Heights[HeightIdx] : Layer.MinHeight;

					FVector UEPos = FWowCoordinate::AdtToUE(NgX, NgY, NgZ);
					Vertices.Add(UEPos);
					UVs.Add(FVector2D((float)Col / (float)Layer.Width, (float)Row / (float)Layer.Height));
				}
			}

			// Build triangles, skipping sub-tiles where liquid doesn't exist
			TArray<int32> Indices;
			for (int32 Row = 0; Row < Layer.Height; Row++)
			{
				for (int32 Col = 0; Col < Layer.Width; Col++)
				{
					// Check existence bitmap
					int32 BitX = Layer.XOffset + Col;
					int32 BitY = Layer.YOffset + Row;
					if (BitX < 8 && BitY < 8)
					{
						uint64 Bit = 1ULL << (BitY * 8 + BitX);
						if ((Layer.ExistsBitmap & Bit) == 0) continue;
					}

					int32 TL = Row * VW + Col;
					int32 TR = TL + 1;
					int32 BL = TL + VW;
					int32 BR = BL + 1;

					Indices.Add(TL);
					Indices.Add(BL);
					Indices.Add(TR);

					Indices.Add(TR);
					Indices.Add(BL);
					Indices.Add(BR);
				}
			}

			if (Indices.Num() == 0) continue;

			// Build UStaticMesh from FMeshDescription
			FMeshDescription MeshDesc;
			FStaticMeshAttributes Attributes(MeshDesc);
			Attributes.Register();

			TVertexAttributesRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
			TVertexInstanceAttributesRef<FVector3f> VertexInstanceNormals = Attributes.GetVertexInstanceNormals();
			TVertexInstanceAttributesRef<FVector3f> VertexInstanceTangents = Attributes.GetVertexInstanceTangents();
			TVertexInstanceAttributesRef<float> VertexInstanceBinormalSigns = Attributes.GetVertexInstanceBinormalSigns();
			TVertexInstanceAttributesRef<FVector2f> VertexInstanceUVs = Attributes.GetVertexInstanceUVs();

			FPolygonGroupID PolyGroup = MeshDesc.CreatePolygonGroup();
			MeshDesc.ReserveNewVertices(Vertices.Num());
			MeshDesc.ReserveNewVertexInstances(Vertices.Num());
			MeshDesc.ReserveNewPolygons(Indices.Num() / 3);

			TArray<FVertexInstanceID> VertexInstanceIDs;
			VertexInstanceIDs.SetNum(Vertices.Num());

			for (int32 v = 0; v < Vertices.Num(); ++v)
			{
				const FVector& P = Vertices[v];
				FVertexID VertID = MeshDesc.CreateVertex();
				VertexPositions[VertID] = FVector3f(P.X, P.Y, P.Z);

				FVertexInstanceID InstID = MeshDesc.CreateVertexInstance(VertID);
				VertexInstanceIDs[v] = InstID;

				VertexInstanceNormals[InstID] = FVector3f(0, 0, 1); // water faces up
				VertexInstanceTangents[InstID] = FVector3f(1, 0, 0);
				VertexInstanceBinormalSigns[InstID] = 1.0f;

				FVector2f UV = (v < UVs.Num()) ? FVector2f(UVs[v]) : FVector2f(0, 0);
				VertexInstanceUVs.Set(InstID, 0, UV);
			}

			for (int32 t = 0; t < Indices.Num(); t += 3)
			{
				TArray<FVertexInstanceID> TriVerts;
				TriVerts.Add(VertexInstanceIDs[Indices[t]]);
				TriVerts.Add(VertexInstanceIDs[Indices[t + 1]]);
				TriVerts.Add(VertexInstanceIDs[Indices[t + 2]]);
				MeshDesc.CreatePolygon(PolyGroup, TriVerts);
			}

			UStaticMesh* SM = NewObject<UStaticMesh>();
			SM->AddToRoot();

			TArray<const FMeshDescription*> MeshDescs;
			MeshDescs.Add(&MeshDesc);
			UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
			BuildParams.bBuildSimpleCollision = false;
			BuildParams.bFastBuild = true;
			SM->BuildFromMeshDescriptions(MeshDescs, BuildParams);

			// Create material based on liquid category
			UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(
				UMaterial::GetDefaultMaterial(MD_Surface), Owner);
			if (Mat)
			{
				int32 Category = Layer.GetLiquidCategory();
				switch (Category)
				{
				case 0: // water
				case 1: // ocean
					Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.1f, 0.3f, 0.6f, 0.6f));
					break;
				case 2: // magma
					Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.9f, 0.3f, 0.05f, 1.0f));
					break;
				case 3: // slime
					Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.2f, 0.6f, 0.1f, 0.8f));
					break;
				}
				SM->SetMaterial(0, Mat);
			}

			// Create UStaticMeshComponent
			FString CompName = FString::Printf(TEXT("Water_%d"), WaterSectionIndex++);
			UStaticMeshComponent* WaterMeshComp = NewObject<UStaticMeshComponent>(Owner, *CompName);
			WaterMeshComp->SetStaticMesh(SM);
			WaterMeshComp->RegisterComponent();
			WaterMeshComp->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			WaterMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WaterMeshComp->SetCastShadow(false);

			Result.Add(WaterMeshComp);
		}
	}

	if (Result.Num() > 0)
	{
		UE_LOG(LogWowWater, Log, TEXT("Created %d water meshes for tile %d,%d"), Result.Num(), TileX, TileY);
	}

	return Result;
}
